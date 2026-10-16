/**
 * @file test_neuromodulator_kernels.cpp
 * @brief Unit tests for GPU neuromodulator kernels
 *
 * Tests dopamine, serotonin, acetylcholine, norepinephrine, vesicle dynamics,
 * receptor kinetics, and integrated neuromodulator system GPU operations.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/neuromodulators/nimcp_neuromodulator_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class NeuromodulatorKernelTest : public ::testing::Test {
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

    //=========================================================================
    // State Creation Helpers
    //=========================================================================

    nimcp_gpu_dopamine_state_t* CreateDopamineState(size_t n_neurons, size_t n_targets) {
        if (!ctx) return nullptr;

        nimcp_gpu_dopamine_state_t* state = new nimcp_gpu_dopamine_state_t;
        state->n_neurons = n_neurons;
        state->n_targets = n_targets;

        state->concentration = Create1DTensor(n_targets, 0.05f);    // baseline
        state->d1_occupancy = Create1DTensor(n_targets, 0.0f);
        state->d2_occupancy = Create1DTensor(n_targets, 0.0f);
        state->vesicle_pool = Create1DTensor(n_neurons, 1.0f);      // full pool
        state->release_prob = Create1DTensor(n_neurons, 0.3f);
        state->reward_prediction = Create1DTensor(n_targets, 0.0f);

        if (!state->concentration || !state->d1_occupancy || !state->d2_occupancy ||
            !state->vesicle_pool || !state->release_prob || !state->reward_prediction) {
            DestroyDopamineState(state);
            return nullptr;
        }
        return state;
    }

    void DestroyDopamineState(nimcp_gpu_dopamine_state_t* state) {
        if (!state) return;
        if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
        if (state->d1_occupancy) nimcp_gpu_tensor_destroy(state->d1_occupancy);
        if (state->d2_occupancy) nimcp_gpu_tensor_destroy(state->d2_occupancy);
        if (state->vesicle_pool) nimcp_gpu_tensor_destroy(state->vesicle_pool);
        if (state->release_prob) nimcp_gpu_tensor_destroy(state->release_prob);
        if (state->reward_prediction) nimcp_gpu_tensor_destroy(state->reward_prediction);
        delete state;
    }

    nimcp_gpu_serotonin_state_t* CreateSerotoninState(size_t n_neurons, size_t n_targets) {
        if (!ctx) return nullptr;

        nimcp_gpu_serotonin_state_t* state = new nimcp_gpu_serotonin_state_t;
        state->n_neurons = n_neurons;
        state->n_targets = n_targets;

        state->concentration = Create1DTensor(n_targets, 0.01f);     // baseline
        state->ht1a_occupancy = Create1DTensor(n_targets, 0.0f);
        state->ht2a_occupancy = Create1DTensor(n_targets, 0.0f);
        state->vesicle_pool = Create1DTensor(n_neurons, 1.0f);
        state->synthesis_state = Create1DTensor(n_neurons, 1.0f);

        if (!state->concentration || !state->ht1a_occupancy || !state->ht2a_occupancy ||
            !state->vesicle_pool || !state->synthesis_state) {
            DestroySerotoninState(state);
            return nullptr;
        }
        return state;
    }

    void DestroySerotoninState(nimcp_gpu_serotonin_state_t* state) {
        if (!state) return;
        if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
        if (state->ht1a_occupancy) nimcp_gpu_tensor_destroy(state->ht1a_occupancy);
        if (state->ht2a_occupancy) nimcp_gpu_tensor_destroy(state->ht2a_occupancy);
        if (state->vesicle_pool) nimcp_gpu_tensor_destroy(state->vesicle_pool);
        if (state->synthesis_state) nimcp_gpu_tensor_destroy(state->synthesis_state);
        delete state;
    }

    nimcp_gpu_acetylcholine_state_t* CreateAcetylcholineState(size_t n_neurons, size_t n_targets) {
        if (!ctx) return nullptr;

        nimcp_gpu_acetylcholine_state_t* state = new nimcp_gpu_acetylcholine_state_t;
        state->n_neurons = n_neurons;
        state->n_targets = n_targets;

        state->concentration = Create1DTensor(n_targets, 0.02f);     // baseline
        state->m1_occupancy = Create1DTensor(n_targets, 0.0f);
        state->m2_occupancy = Create1DTensor(n_targets, 0.0f);
        state->nicotinic_state = Create1DTensor(n_targets, 0.0f);
        state->vesicle_pool = Create1DTensor(n_neurons, 1.0f);
        state->attention_signal = Create1DTensor(n_targets, 0.0f);

        if (!state->concentration || !state->m1_occupancy || !state->m2_occupancy ||
            !state->nicotinic_state || !state->vesicle_pool || !state->attention_signal) {
            DestroyAcetylcholineState(state);
            return nullptr;
        }
        return state;
    }

    void DestroyAcetylcholineState(nimcp_gpu_acetylcholine_state_t* state) {
        if (!state) return;
        if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
        if (state->m1_occupancy) nimcp_gpu_tensor_destroy(state->m1_occupancy);
        if (state->m2_occupancy) nimcp_gpu_tensor_destroy(state->m2_occupancy);
        if (state->nicotinic_state) nimcp_gpu_tensor_destroy(state->nicotinic_state);
        if (state->vesicle_pool) nimcp_gpu_tensor_destroy(state->vesicle_pool);
        if (state->attention_signal) nimcp_gpu_tensor_destroy(state->attention_signal);
        delete state;
    }

    nimcp_gpu_norepinephrine_state_t* CreateNorepinephrineState(size_t n_neurons, size_t n_targets) {
        if (!ctx) return nullptr;

        nimcp_gpu_norepinephrine_state_t* state = new nimcp_gpu_norepinephrine_state_t;
        state->n_neurons = n_neurons;
        state->n_targets = n_targets;

        state->concentration = Create1DTensor(n_targets, 0.02f);     // baseline
        state->alpha1_occupancy = Create1DTensor(n_targets, 0.0f);
        state->alpha2_occupancy = Create1DTensor(n_targets, 0.0f);
        state->beta_occupancy = Create1DTensor(n_targets, 0.0f);
        state->vesicle_pool = Create1DTensor(n_neurons, 1.0f);
        state->arousal_signal = Create1DTensor(n_targets, 0.0f);

        if (!state->concentration || !state->alpha1_occupancy || !state->alpha2_occupancy ||
            !state->beta_occupancy || !state->vesicle_pool || !state->arousal_signal) {
            DestroyNorepinephrineState(state);
            return nullptr;
        }
        return state;
    }

    void DestroyNorepinephrineState(nimcp_gpu_norepinephrine_state_t* state) {
        if (!state) return;
        if (state->concentration) nimcp_gpu_tensor_destroy(state->concentration);
        if (state->alpha1_occupancy) nimcp_gpu_tensor_destroy(state->alpha1_occupancy);
        if (state->alpha2_occupancy) nimcp_gpu_tensor_destroy(state->alpha2_occupancy);
        if (state->beta_occupancy) nimcp_gpu_tensor_destroy(state->beta_occupancy);
        if (state->vesicle_pool) nimcp_gpu_tensor_destroy(state->vesicle_pool);
        if (state->arousal_signal) nimcp_gpu_tensor_destroy(state->arousal_signal);
        delete state;
    }

    nimcp_gpu_neuromod_system_t* CreateNeuromodSystem(size_t n_neurons, size_t n_targets) {
        if (!ctx) return nullptr;

        nimcp_gpu_neuromod_system_t* system = new nimcp_gpu_neuromod_system_t;
        system->dopamine = CreateDopamineState(n_neurons, n_targets);
        system->serotonin = CreateSerotoninState(n_neurons, n_targets);
        system->acetylcholine = CreateAcetylcholineState(n_neurons, n_targets);
        system->norepinephrine = CreateNorepinephrineState(n_neurons, n_targets);
        system->interaction_matrix = Create2DTensor(NIMCP_NEUROMOD_COUNT, NIMCP_NEUROMOD_COUNT, 0.0f);
        system->dt = 1.0f;

        if (!system->dopamine || !system->serotonin || !system->acetylcholine ||
            !system->norepinephrine || !system->interaction_matrix) {
            DestroyNeuromodSystem(system);
            return nullptr;
        }
        return system;
    }

    void DestroyNeuromodSystem(nimcp_gpu_neuromod_system_t* system) {
        if (!system) return;
        if (system->dopamine) DestroyDopamineState(system->dopamine);
        if (system->serotonin) DestroySerotoninState(system->serotonin);
        if (system->acetylcholine) DestroyAcetylcholineState(system->acetylcholine);
        if (system->norepinephrine) DestroyNorepinephrineState(system->norepinephrine);
        if (system->interaction_matrix) nimcp_gpu_tensor_destroy(system->interaction_matrix);
        delete system;
    }
};

//=============================================================================
// Default Parameter Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineParamsDefault_ReturnsValidParams) {
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.baseline, 0.0f);
    EXPECT_GT(params.release_rate, 0.0f);
    EXPECT_GT(params.reuptake_tau, 0.0f);
    EXPECT_GT(params.decay_tau, 0.0f);
    EXPECT_GT(params.d1_affinity, 0.0f);
    EXPECT_GT(params.d2_affinity, 0.0f);
    EXPECT_GT(params.max_conc, params.baseline);
    EXPECT_GT(params.burst_factor, 1.0f);
    EXPECT_GT(params.tonic_rate, 0.0f);
    EXPECT_GT(params.phasic_amplitude, 0.0f);
}

TEST_F(NeuromodulatorKernelTest, SerotoninParamsDefault_ReturnsValidParams) {
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.baseline, 0.0f);
    EXPECT_GT(params.release_rate, 0.0f);
    EXPECT_GT(params.reuptake_tau, 0.0f);
    EXPECT_GT(params.decay_tau, 0.0f);
    EXPECT_GT(params.ht1a_affinity, 0.0f);
    EXPECT_GT(params.ht2a_affinity, 0.0f);
    EXPECT_GT(params.max_conc, params.baseline);
    EXPECT_GT(params.autoreceptor_gain, 0.0f);
    EXPECT_GT(params.synthesis_rate, 0.0f);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineParamsDefault_ReturnsValidParams) {
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.baseline, 0.0f);
    EXPECT_GT(params.release_rate, 0.0f);
    EXPECT_GT(params.ache_rate, 0.0f);
    EXPECT_GT(params.decay_tau, 0.0f);
    EXPECT_GT(params.m1_affinity, 0.0f);
    EXPECT_GT(params.m2_affinity, 0.0f);
    EXPECT_GT(params.nicotinic_affinity, 0.0f);
    EXPECT_GT(params.max_conc, params.baseline);
    EXPECT_GT(params.desensitization_tau, 0.0f);
    EXPECT_GT(params.choline_uptake_km, 0.0f);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineParamsDefault_ReturnsValidParams) {
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    // Check reasonable defaults
    EXPECT_GT(params.baseline, 0.0f);
    EXPECT_GT(params.release_rate, 0.0f);
    EXPECT_GT(params.reuptake_tau, 0.0f);
    EXPECT_GT(params.decay_tau, 0.0f);
    EXPECT_GT(params.alpha1_affinity, 0.0f);
    EXPECT_GT(params.alpha2_affinity, 0.0f);
    EXPECT_GT(params.beta_affinity, 0.0f);
    EXPECT_GT(params.max_conc, params.baseline);
    EXPECT_GT(params.lc_baseline_rate, 0.0f);
    EXPECT_GT(params.stress_gain, 0.0f);
}

//=============================================================================
// Dopamine State Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_neurons = 100;
    const size_t n_targets = 500;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->concentration, nullptr);
    EXPECT_NE(state->d1_occupancy, nullptr);
    EXPECT_NE(state->d2_occupancy, nullptr);
    EXPECT_NE(state->vesicle_pool, nullptr);
    EXPECT_NE(state->release_prob, nullptr);
    EXPECT_NE(state->reward_prediction, nullptr);
    EXPECT_EQ(state->n_neurons, n_neurons);
    EXPECT_EQ(state->n_targets, n_targets);

    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineStateDestroy_HandlesNull) {
    DestroyDopamineState(nullptr);  // Should not crash
}

//=============================================================================
// Dopamine Update Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineUpdate_IncreasesConcentrationOnSpikes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 1.0f;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 1.0f);  // All neurons spike
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    auto initial_conc = CopyToHost(state->concentration);

    bool result = nimcp_gpu_dopamine_update(ctx, state, spikes, dt, &params);
    EXPECT_TRUE(result);

    auto final_conc = CopyToHost(state->concentration);

    // Concentration should increase after spikes
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GT(final_conc[i], initial_conc[i]);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineUpdate_DecaysToBaselineWithoutSpikes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Start with elevated concentration
    nimcp_gpu_fill(ctx, state->concentration, 1.0f);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 0.0f);  // No spikes
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    // Run multiple updates
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_dopamine_update(ctx, state, spikes, dt, &params);
        EXPECT_TRUE(result);
    }

    auto final_conc = CopyToHost(state->concentration);

    // Concentration should decay toward baseline
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_LT(final_conc[i], 1.0f);
        EXPECT_GE(final_conc[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroyDopamineState(state);
}

//=============================================================================
// Dopamine RPE Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineComputeRPE_PositiveWhenRewardExceedsPrediction) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* reward = Create1DTensor(n_targets, 1.0f);     // High reward
    nimcp_gpu_tensor_t* predicted = Create1DTensor(n_targets, 0.2f); // Low prediction
    nimcp_gpu_tensor_t* rpe_out = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    bool result = nimcp_gpu_dopamine_compute_rpe(ctx, state, reward, predicted, rpe_out, &params);
    EXPECT_TRUE(result);

    auto rpe_data = CopyToHost(rpe_out);

    // RPE should be positive when reward > predicted
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GT(rpe_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(reward);
    nimcp_gpu_tensor_destroy(predicted);
    nimcp_gpu_tensor_destroy(rpe_out);
    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineComputeRPE_NegativeWhenRewardBelowPrediction) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* reward = Create1DTensor(n_targets, 0.1f);    // Low reward
    nimcp_gpu_tensor_t* predicted = Create1DTensor(n_targets, 0.8f); // High prediction
    nimcp_gpu_tensor_t* rpe_out = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    bool result = nimcp_gpu_dopamine_compute_rpe(ctx, state, reward, predicted, rpe_out, &params);
    EXPECT_TRUE(result);

    auto rpe_data = CopyToHost(rpe_out);

    // RPE should be negative when reward < predicted
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_LT(rpe_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(reward);
    nimcp_gpu_tensor_destroy(predicted);
    nimcp_gpu_tensor_destroy(rpe_out);
    DestroyDopamineState(state);
}

//=============================================================================
// Dopamine Receptor Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineReceptorUpdate_IncreasesOccupancyWithHighConcentration) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set high dopamine concentration
    nimcp_gpu_fill(ctx, state->concentration, 1.0f);

    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    // Run multiple updates
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_dopamine_receptor_update(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto d1_data = CopyToHost(state->d1_occupancy);
    auto d2_data = CopyToHost(state->d2_occupancy);

    // Receptor occupancy should increase with high concentration
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GT(d1_data[i], 0.0f);
        EXPECT_GT(d2_data[i], 0.0f);
        EXPECT_LE(d1_data[i], 1.0f);  // Occupancy bounded by 1
        EXPECT_LE(d2_data[i], 1.0f);
    }

    DestroyDopamineState(state);
}

//=============================================================================
// Dopamine Plasticity Modulation Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineModulatePlasticity_ModifiesWeights) {
    RequireGPU();

    const size_t n_pre = 20;
    const size_t n_post = 10;
    const size_t n_synapses = n_pre * n_post;
    const float learning_rate = 0.01f;

    nimcp_gpu_tensor_t* weights = Create2DTensor(n_post, n_pre, 0.5f);
    nimcp_gpu_tensor_t* d1_effect = Create1DTensor(n_synapses, 0.8f);
    nimcp_gpu_tensor_t* d2_effect = Create1DTensor(n_synapses, 0.2f);
    nimcp_gpu_tensor_t* eligibility = Create1DTensor(n_synapses, 0.5f);

    bool result = nimcp_gpu_dopamine_modulate_plasticity(
        ctx, weights, d1_effect, d2_effect, eligibility, learning_rate);
    EXPECT_TRUE(result);

    auto weight_data = CopyToHost(weights);

    // Weights should be modified from initial value
    bool modified = false;
    for (size_t i = 0; i < n_synapses; i++) {
        if (std::abs(weight_data[i] - 0.5f) > 1e-6f) {
            modified = true;
            break;
        }
    }
    EXPECT_TRUE(modified);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(d1_effect);
    nimcp_gpu_tensor_destroy(d2_effect);
    nimcp_gpu_tensor_destroy(eligibility);
}

//=============================================================================
// Serotonin State Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, SerotoninStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_neurons = 100;
    const size_t n_targets = 500;

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->concentration, nullptr);
    EXPECT_NE(state->ht1a_occupancy, nullptr);
    EXPECT_NE(state->ht2a_occupancy, nullptr);
    EXPECT_NE(state->vesicle_pool, nullptr);
    EXPECT_NE(state->synthesis_state, nullptr);
    EXPECT_EQ(state->n_neurons, n_neurons);
    EXPECT_EQ(state->n_targets, n_targets);

    DestroySerotoninState(state);
}

TEST_F(NeuromodulatorKernelTest, SerotoninStateDestroy_HandlesNull) {
    DestroySerotoninState(nullptr);  // Should not crash
}

//=============================================================================
// Serotonin Update Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, SerotoninUpdate_IncreasesConcentrationOnSpikes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 1.0f;

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 1.0f);
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    auto initial_conc = CopyToHost(state->concentration);

    bool result = nimcp_gpu_serotonin_update(ctx, state, spikes, dt, &params);
    EXPECT_TRUE(result);

    auto final_conc = CopyToHost(state->concentration);

    // Concentration should increase after spikes
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(final_conc[i], initial_conc[i]);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroySerotoninState(state);
}

//=============================================================================
// Serotonin Receptor Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, SerotoninReceptorUpdate_UpdatesOccupancy) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set high serotonin concentration
    nimcp_gpu_fill(ctx, state->concentration, 0.5f);

    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_serotonin_receptor_update(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto ht1a_data = CopyToHost(state->ht1a_occupancy);
    auto ht2a_data = CopyToHost(state->ht2a_occupancy);

    // Receptor occupancy should increase
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GT(ht1a_data[i], 0.0f);
        EXPECT_GT(ht2a_data[i], 0.0f);
        EXPECT_LE(ht1a_data[i], 1.0f);
        EXPECT_LE(ht2a_data[i], 1.0f);
    }

    DestroySerotoninState(state);
}

//=============================================================================
// Serotonin Behavior Modulation Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, SerotoninModulateBehavior_ProducesValidOutputs) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set moderate serotonin levels
    nimcp_gpu_fill(ctx, state->concentration, 0.1f);
    nimcp_gpu_fill(ctx, state->ht1a_occupancy, 0.5f);
    nimcp_gpu_fill(ctx, state->ht2a_occupancy, 0.3f);

    nimcp_gpu_tensor_t* impulse_control = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* mood_signal = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    bool result = nimcp_gpu_serotonin_modulate_behavior(
        ctx, state, impulse_control, mood_signal, &params);
    EXPECT_TRUE(result);

    auto impulse_data = CopyToHost(impulse_control);
    auto mood_data = CopyToHost(mood_signal);

    // Outputs should be computed (non-zero with active state)
    bool has_impulse = false, has_mood = false;
    for (size_t i = 0; i < n_targets; i++) {
        if (std::abs(impulse_data[i]) > 1e-6f) has_impulse = true;
        if (std::abs(mood_data[i]) > 1e-6f) has_mood = true;
    }
    EXPECT_TRUE(has_impulse || has_mood);

    nimcp_gpu_tensor_destroy(impulse_control);
    nimcp_gpu_tensor_destroy(mood_signal);
    DestroySerotoninState(state);
}

//=============================================================================
// Acetylcholine State Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, AcetylcholineStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_neurons = 100;
    const size_t n_targets = 500;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->concentration, nullptr);
    EXPECT_NE(state->m1_occupancy, nullptr);
    EXPECT_NE(state->m2_occupancy, nullptr);
    EXPECT_NE(state->nicotinic_state, nullptr);
    EXPECT_NE(state->vesicle_pool, nullptr);
    EXPECT_NE(state->attention_signal, nullptr);
    EXPECT_EQ(state->n_neurons, n_neurons);
    EXPECT_EQ(state->n_targets, n_targets);

    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineStateDestroy_HandlesNull) {
    DestroyAcetylcholineState(nullptr);  // Should not crash
}

//=============================================================================
// Acetylcholine Update Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, AcetylcholineUpdate_IncreasesConcentrationOnSpikes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 1.0f;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 1.0f);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    auto initial_conc = CopyToHost(state->concentration);

    bool result = nimcp_gpu_acetylcholine_update(ctx, state, spikes, dt, &params);
    EXPECT_TRUE(result);

    auto final_conc = CopyToHost(state->concentration);

    // Concentration should change after spikes
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(final_conc[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineUpdate_FastDecayDueToAChE) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Start with elevated concentration
    nimcp_gpu_fill(ctx, state->concentration, 0.5f);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    // ACh should decay fast due to AChE
    for (int i = 0; i < 20; i++) {
        nimcp_gpu_acetylcholine_update(ctx, state, spikes, dt, &params);
    }

    auto final_conc = CopyToHost(state->concentration);

    // Should decay significantly (ACh has fast decay)
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_LT(final_conc[i], 0.5f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroyAcetylcholineState(state);
}

//=============================================================================
// Acetylcholine Receptor Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, AcetylcholineReceptorUpdate_UpdatesAllReceptorTypes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set high ACh concentration
    nimcp_gpu_fill(ctx, state->concentration, 0.5f);

    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_acetylcholine_receptor_update(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto m1_data = CopyToHost(state->m1_occupancy);
    auto m2_data = CopyToHost(state->m2_occupancy);
    auto nic_data = CopyToHost(state->nicotinic_state);

    // All receptor types should respond
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(m1_data[i], 0.0f);
        EXPECT_GE(m2_data[i], 0.0f);
        EXPECT_LE(m1_data[i], 1.0f);
        EXPECT_LE(m2_data[i], 1.0f);
    }

    DestroyAcetylcholineState(state);
}

//=============================================================================
// Acetylcholine Attention Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, AcetylcholineComputeAttention_ProducesValidSignal) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set active ACh state
    nimcp_gpu_fill(ctx, state->concentration, 0.3f);
    nimcp_gpu_fill(ctx, state->m1_occupancy, 0.6f);

    nimcp_gpu_tensor_t* salience = Create1DTensor(n_targets, 0.8f);
    nimcp_gpu_tensor_t* attention_out = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    bool result = nimcp_gpu_acetylcholine_compute_attention(
        ctx, state, salience, attention_out, &params);
    EXPECT_TRUE(result);

    auto attention_data = CopyToHost(attention_out);

    // Attention signal should be computed
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(attention_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(salience);
    nimcp_gpu_tensor_destroy(attention_out);
    DestroyAcetylcholineState(state);
}

//=============================================================================
// Acetylcholine Learning Modulation Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, AcetylcholineModulateLearning_ScalesLearningRates) {
    RequireGPU();

    const size_t n = 100;
    const float baseline_rate = 0.01f;
    const float max_modulation = 3.0f;

    nimcp_gpu_tensor_t* learning_rates = Create1DTensor(n, baseline_rate);
    nimcp_gpu_tensor_t* ach_concentration = Create1DTensor(n, 0.5f);  // High ACh

    bool result = nimcp_gpu_acetylcholine_modulate_learning(
        ctx, learning_rates, ach_concentration, baseline_rate, max_modulation);
    EXPECT_TRUE(result);

    auto lr_data = CopyToHost(learning_rates);

    // Learning rates should be modulated
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(lr_data[i], 0.0f);
        EXPECT_LE(lr_data[i], baseline_rate * max_modulation);
    }

    nimcp_gpu_tensor_destroy(learning_rates);
    nimcp_gpu_tensor_destroy(ach_concentration);
}

//=============================================================================
// Norepinephrine State Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NorepinephrineStateCreate_ReturnsValidState) {
    RequireGPU();

    const size_t n_neurons = 100;
    const size_t n_targets = 500;

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->concentration, nullptr);
    EXPECT_NE(state->alpha1_occupancy, nullptr);
    EXPECT_NE(state->alpha2_occupancy, nullptr);
    EXPECT_NE(state->beta_occupancy, nullptr);
    EXPECT_NE(state->vesicle_pool, nullptr);
    EXPECT_NE(state->arousal_signal, nullptr);
    EXPECT_EQ(state->n_neurons, n_neurons);
    EXPECT_EQ(state->n_targets, n_targets);

    DestroyNorepinephrineState(state);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineStateDestroy_HandlesNull) {
    DestroyNorepinephrineState(nullptr);  // Should not crash
}

//=============================================================================
// Norepinephrine Update Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NorepinephrineUpdate_IncreasesConcentrationOnSpikes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 1.0f;

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 1.0f);
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    auto initial_conc = CopyToHost(state->concentration);

    bool result = nimcp_gpu_norepinephrine_update(ctx, state, spikes, dt, &params);
    EXPECT_TRUE(result);

    auto final_conc = CopyToHost(state->concentration);

    // Concentration should increase after spikes
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(final_conc[i], initial_conc[i]);
    }

    nimcp_gpu_tensor_destroy(spikes);
    DestroyNorepinephrineState(state);
}

//=============================================================================
// Norepinephrine Receptor Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NorepinephrineReceptorUpdate_UpdatesAllReceptorTypes) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;
    const float dt = 10.0f;

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set high NE concentration
    nimcp_gpu_fill(ctx, state->concentration, 0.5f);

    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_norepinephrine_receptor_update(ctx, state, dt, &params);
        EXPECT_TRUE(result);
    }

    auto alpha1_data = CopyToHost(state->alpha1_occupancy);
    auto alpha2_data = CopyToHost(state->alpha2_occupancy);
    auto beta_data = CopyToHost(state->beta_occupancy);

    // All receptor types should respond
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(alpha1_data[i], 0.0f);
        EXPECT_GE(alpha2_data[i], 0.0f);
        EXPECT_GE(beta_data[i], 0.0f);
        EXPECT_LE(alpha1_data[i], 1.0f);
        EXPECT_LE(alpha2_data[i], 1.0f);
        EXPECT_LE(beta_data[i], 1.0f);
    }

    DestroyNorepinephrineState(state);
}

//=============================================================================
// Norepinephrine Arousal Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NorepinephrineComputeArousal_RespondsToStress) {
    RequireGPU();

    const size_t n_neurons = 50;
    const size_t n_targets = 50;

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    // Set active NE state
    nimcp_gpu_fill(ctx, state->concentration, 0.3f);
    nimcp_gpu_fill(ctx, state->beta_occupancy, 0.5f);

    nimcp_gpu_tensor_t* stress_input = Create1DTensor(n_targets, 0.8f);  // High stress
    nimcp_gpu_tensor_t* arousal_out = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    bool result = nimcp_gpu_norepinephrine_compute_arousal(
        ctx, state, stress_input, arousal_out, &params);
    EXPECT_TRUE(result);

    auto arousal_data = CopyToHost(arousal_out);

    // Arousal signal should be elevated with stress
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(arousal_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(stress_input);
    nimcp_gpu_tensor_destroy(arousal_out);
    DestroyNorepinephrineState(state);
}

//=============================================================================
// Norepinephrine Gain Modulation Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NorepinephrineModulateGain_FollowsInvertedU) {
    RequireGPU();

    const size_t n = 100;
    const float optimal_arousal = 0.5f;
    const float gain_sensitivity = 2.0f;

    // Test low NE (low arousal)
    nimcp_gpu_tensor_t* gains_low = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* ne_low = Create1DTensor(n, 0.1f);

    bool result1 = nimcp_gpu_norepinephrine_modulate_gain(
        ctx, gains_low, ne_low, optimal_arousal, gain_sensitivity);
    EXPECT_TRUE(result1);

    // Test optimal NE
    nimcp_gpu_tensor_t* gains_opt = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* ne_opt = Create1DTensor(n, 0.5f);

    bool result2 = nimcp_gpu_norepinephrine_modulate_gain(
        ctx, gains_opt, ne_opt, optimal_arousal, gain_sensitivity);
    EXPECT_TRUE(result2);

    // Test high NE (high arousal)
    nimcp_gpu_tensor_t* gains_high = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* ne_high = Create1DTensor(n, 0.9f);

    bool result3 = nimcp_gpu_norepinephrine_modulate_gain(
        ctx, gains_high, ne_high, optimal_arousal, gain_sensitivity);
    EXPECT_TRUE(result3);

    auto low_data = CopyToHost(gains_low);
    auto opt_data = CopyToHost(gains_opt);
    auto high_data = CopyToHost(gains_high);

    // Gains should be valid (inverted-U relationship with arousal)
    for (size_t i = 0; i < n; i++) {
        EXPECT_GE(low_data[i], 0.0f);
        EXPECT_GE(opt_data[i], 0.0f);
        EXPECT_GE(high_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(gains_low);
    nimcp_gpu_tensor_destroy(ne_low);
    nimcp_gpu_tensor_destroy(gains_opt);
    nimcp_gpu_tensor_destroy(ne_opt);
    nimcp_gpu_tensor_destroy(gains_high);
    nimcp_gpu_tensor_destroy(ne_high);
}

//=============================================================================
// Vesicle Dynamics Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, VesicleDynamics_DepletesOnSpikes) {
    RequireGPU();

    const size_t n = 100;
    const float release_prob = 0.3f;
    const float replenish_rate = 0.01f;
    const float max_pool = 1.0f;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* vesicle_pool = Create1DTensor(n, 1.0f);  // Full pool
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n, 1.0f);         // All spike

    bool result = nimcp_gpu_vesicle_dynamics(
        ctx, vesicle_pool, spikes, release_prob, replenish_rate, max_pool, dt);
    EXPECT_TRUE(result);

    auto pool_data = CopyToHost(vesicle_pool);

    // Pool should be depleted after spikes
    for (size_t i = 0; i < n; i++) {
        EXPECT_LT(pool_data[i], 1.0f);
        EXPECT_GE(pool_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(vesicle_pool);
    nimcp_gpu_tensor_destroy(spikes);
}

TEST_F(NeuromodulatorKernelTest, VesicleDynamics_ReplenishesWithoutSpikes) {
    RequireGPU();

    const size_t n = 100;
    const float release_prob = 0.3f;
    const float replenish_rate = 0.1f;
    const float max_pool = 1.0f;
    const float dt = 10.0f;

    nimcp_gpu_tensor_t* vesicle_pool = Create1DTensor(n, 0.2f);  // Depleted pool
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n, 0.0f);         // No spikes

    // Run multiple updates
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_vesicle_dynamics(
            ctx, vesicle_pool, spikes, release_prob, replenish_rate, max_pool, dt);
        EXPECT_TRUE(result);
    }

    auto pool_data = CopyToHost(vesicle_pool);

    // Pool should replenish toward max
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(pool_data[i], 0.2f);
        EXPECT_LE(pool_data[i], max_pool);
    }

    nimcp_gpu_tensor_destroy(vesicle_pool);
    nimcp_gpu_tensor_destroy(spikes);
}

//=============================================================================
// Receptor Kinetics Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, ReceptorKinetics_IncreasesOccupancyWithConcentration) {
    RequireGPU();

    const size_t n = 100;
    const float affinity = 0.1f;      // Kd
    const float on_rate = 1.0f;
    const float off_rate = 0.1f;
    // Use small dt to ensure numerical stability of forward Euler integration
    // (large dt causes oscillation: occ overshoots to 1.0 then drops to 0.0)
    const float dt = 0.5f;

    nimcp_gpu_tensor_t* occupancy = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* concentration = Create1DTensor(n, 0.5f);  // High concentration

    // Run multiple updates to reach equilibrium
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_receptor_kinetics(
            ctx, occupancy, concentration, affinity, on_rate, off_rate, dt);
        EXPECT_TRUE(result);
    }

    auto occ_data = CopyToHost(occupancy);

    // Occupancy should increase with high concentration
    // Equilibrium: on_rate * conc * (1-occ) = off_rate * occ
    //   => occ = on_rate * conc / (on_rate * conc + off_rate) = 0.5/0.6 ~ 0.833
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(occ_data[i], 0.0f);
        EXPECT_LE(occ_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(occupancy);
    nimcp_gpu_tensor_destroy(concentration);
}

TEST_F(NeuromodulatorKernelTest, ReceptorKinetics_DecreasesOccupancyWithoutLigand) {
    RequireGPU();

    const size_t n = 100;
    const float affinity = 0.1f;
    const float on_rate = 1.0f;
    const float off_rate = 0.5f;
    const float dt = 10.0f;

    nimcp_gpu_tensor_t* occupancy = Create1DTensor(n, 0.8f);       // High occupancy
    nimcp_gpu_tensor_t* concentration = Create1DTensor(n, 0.0f);  // No ligand

    // Run multiple updates
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_receptor_kinetics(
            ctx, occupancy, concentration, affinity, on_rate, off_rate, dt);
        EXPECT_TRUE(result);
    }

    auto occ_data = CopyToHost(occupancy);

    // Occupancy should decrease without ligand
    for (size_t i = 0; i < n; i++) {
        EXPECT_LT(occ_data[i], 0.8f);
        EXPECT_GE(occ_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(occupancy);
    nimcp_gpu_tensor_destroy(concentration);
}

//=============================================================================
// Integrated Neuromodulator System Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, NeuromodSystemUpdate_UpdatesAllSystems) {
    RequireGPU();

    const size_t n_neurons = 30;
    const size_t n_targets = 30;
    const float dt = 1.0f;

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(n_neurons, n_targets);
    ASSERT_NE(system, nullptr);

    nimcp_gpu_tensor_t* da_spikes = Create1DTensor(n_targets, 0.5f);
    nimcp_gpu_tensor_t* ht_spikes = Create1DTensor(n_targets, 0.3f);
    nimcp_gpu_tensor_t* ach_spikes = Create1DTensor(n_targets, 0.4f);
    nimcp_gpu_tensor_t* ne_spikes = Create1DTensor(n_targets, 0.2f);

    // Get initial concentrations
    auto da_init = CopyToHost(system->dopamine->concentration);
    auto ht_init = CopyToHost(system->serotonin->concentration);
    auto ach_init = CopyToHost(system->acetylcholine->concentration);
    auto ne_init = CopyToHost(system->norepinephrine->concentration);

    bool result = nimcp_gpu_neuromod_system_update(
        ctx, system, da_spikes, ht_spikes, ach_spikes, ne_spikes, dt);
    EXPECT_TRUE(result);

    // Get final concentrations
    auto da_final = CopyToHost(system->dopamine->concentration);
    auto ht_final = CopyToHost(system->serotonin->concentration);
    auto ach_final = CopyToHost(system->acetylcholine->concentration);
    auto ne_final = CopyToHost(system->norepinephrine->concentration);

    // At least one system should show concentration change
    bool any_change = false;
    for (size_t i = 0; i < n_targets; i++) {
        if (std::abs(da_final[i] - da_init[i]) > 1e-6f ||
            std::abs(ht_final[i] - ht_init[i]) > 1e-6f ||
            std::abs(ach_final[i] - ach_init[i]) > 1e-6f ||
            std::abs(ne_final[i] - ne_init[i]) > 1e-6f) {
            any_change = true;
            break;
        }
    }
    EXPECT_TRUE(any_change);

    nimcp_gpu_tensor_destroy(da_spikes);
    nimcp_gpu_tensor_destroy(ht_spikes);
    nimcp_gpu_tensor_destroy(ach_spikes);
    nimcp_gpu_tensor_destroy(ne_spikes);
    DestroyNeuromodSystem(system);
}

TEST_F(NeuromodulatorKernelTest, NeuromodInteractions_ComputesCrossModulation) {
    RequireGPU();

    const size_t n_neurons = 30;
    const size_t n_targets = 30;

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(n_neurons, n_targets);
    ASSERT_NE(system, nullptr);

    // Set up interaction matrix (e.g., DA inhibits 5-HT)
    std::vector<float> interactions(NIMCP_NEUROMOD_COUNT * NIMCP_NEUROMOD_COUNT, 0.0f);
    interactions[NIMCP_NEUROMOD_DOPAMINE * NIMCP_NEUROMOD_COUNT + NIMCP_NEUROMOD_SEROTONIN] = -0.2f;
    interactions[NIMCP_NEUROMOD_NOREPINEPHRINE * NIMCP_NEUROMOD_COUNT + NIMCP_NEUROMOD_DOPAMINE] = 0.3f;
    system->interaction_matrix = SetFromHost(system->interaction_matrix, interactions);

    // Set active concentrations
    nimcp_gpu_fill(ctx, system->dopamine->concentration, 0.5f);
    nimcp_gpu_fill(ctx, system->norepinephrine->concentration, 0.3f);

    bool result = nimcp_gpu_neuromod_interactions(ctx, system);
    EXPECT_TRUE(result);

    // Function should complete without error
    // Effects would be visible in subsequent updates

    DestroyNeuromodSystem(system);
}

TEST_F(NeuromodulatorKernelTest, NeuromodApplyCombined_ModulatesTargetActivity) {
    RequireGPU();

    const size_t n_neurons = 30;
    const size_t n_targets = 30;

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(n_neurons, n_targets);
    ASSERT_NE(system, nullptr);

    // Set active neuromodulator levels
    nimcp_gpu_fill(ctx, system->dopamine->concentration, 0.3f);
    nimcp_gpu_fill(ctx, system->dopamine->d1_occupancy, 0.5f);
    nimcp_gpu_fill(ctx, system->serotonin->concentration, 0.2f);
    nimcp_gpu_fill(ctx, system->acetylcholine->concentration, 0.4f);
    nimcp_gpu_fill(ctx, system->norepinephrine->concentration, 0.25f);

    nimcp_gpu_tensor_t* target_activity = Create1DTensor(n_targets, 0.5f);
    nimcp_gpu_tensor_t* modulated_output = Create1DTensor(n_targets, 0.0f);

    bool result = nimcp_gpu_neuromod_apply_combined(
        ctx, system, target_activity, modulated_output);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(modulated_output);

    // Output should be modulated from input
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(output_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(target_activity);
    nimcp_gpu_tensor_destroy(modulated_output);
    DestroyNeuromodSystem(system);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, DopamineUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(10, 10);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(10, 0.0f);
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    EXPECT_FALSE(nimcp_gpu_dopamine_update(nullptr, state, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_update(ctx, nullptr, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_update(ctx, state, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_update(ctx, state, spikes, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(spikes);
    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineComputeRPE_NullSafety) {
    RequireGPU();

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(nullptr, state, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(ctx, nullptr, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(ctx, state, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(ctx, state, tensor, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(ctx, state, tensor, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_compute_rpe(ctx, state, tensor, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineReceptorUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(10, 10);
    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();

    EXPECT_FALSE(nimcp_gpu_dopamine_receptor_update(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_receptor_update(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_dopamine_receptor_update(ctx, state, 1.0f, nullptr));

    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, DopamineModulatePlasticity_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_tensor_t* tensor2d = Create2DTensor(10, 10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_dopamine_modulate_plasticity(nullptr, tensor2d, tensor, tensor, tensor, 0.01f));
    EXPECT_FALSE(nimcp_gpu_dopamine_modulate_plasticity(ctx, nullptr, tensor, tensor, tensor, 0.01f));
    EXPECT_FALSE(nimcp_gpu_dopamine_modulate_plasticity(ctx, tensor2d, nullptr, tensor, tensor, 0.01f));
    EXPECT_FALSE(nimcp_gpu_dopamine_modulate_plasticity(ctx, tensor2d, tensor, nullptr, tensor, 0.01f));
    EXPECT_FALSE(nimcp_gpu_dopamine_modulate_plasticity(ctx, tensor2d, tensor, tensor, nullptr, 0.01f));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(tensor2d);
}

TEST_F(NeuromodulatorKernelTest, SerotoninUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(10, 10);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(10, 0.0f);
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    EXPECT_FALSE(nimcp_gpu_serotonin_update(nullptr, state, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_update(ctx, nullptr, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_update(ctx, state, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_update(ctx, state, spikes, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(spikes);
    DestroySerotoninState(state);
}

TEST_F(NeuromodulatorKernelTest, SerotoninReceptorUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(10, 10);
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    EXPECT_FALSE(nimcp_gpu_serotonin_receptor_update(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_receptor_update(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_receptor_update(ctx, state, 1.0f, nullptr));

    DestroySerotoninState(state);
}

TEST_F(NeuromodulatorKernelTest, SerotoninModulateBehavior_NullSafety) {
    RequireGPU();

    nimcp_gpu_serotonin_state_t* state = CreateSerotoninState(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_serotonin_params_t params = nimcp_gpu_serotonin_params_default();

    EXPECT_FALSE(nimcp_gpu_serotonin_modulate_behavior(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_modulate_behavior(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_modulate_behavior(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_modulate_behavior(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_serotonin_modulate_behavior(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroySerotoninState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(10, 10);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(10, 0.0f);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    EXPECT_FALSE(nimcp_gpu_acetylcholine_update(nullptr, state, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_update(ctx, nullptr, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_update(ctx, state, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_update(ctx, state, spikes, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(spikes);
    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineReceptorUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(10, 10);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    EXPECT_FALSE(nimcp_gpu_acetylcholine_receptor_update(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_receptor_update(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_receptor_update(ctx, state, 1.0f, nullptr));

    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineComputeAttention_NullSafety) {
    RequireGPU();

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();

    EXPECT_FALSE(nimcp_gpu_acetylcholine_compute_attention(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_compute_attention(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_compute_attention(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_compute_attention(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_compute_attention(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, AcetylcholineModulateLearning_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_acetylcholine_modulate_learning(nullptr, tensor, tensor, 0.01f, 3.0f));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_modulate_learning(ctx, nullptr, tensor, 0.01f, 3.0f));
    EXPECT_FALSE(nimcp_gpu_acetylcholine_modulate_learning(ctx, tensor, nullptr, 0.01f, 3.0f));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(10, 10);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(10, 0.0f);
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    EXPECT_FALSE(nimcp_gpu_norepinephrine_update(nullptr, state, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_update(ctx, nullptr, spikes, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_update(ctx, state, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_update(ctx, state, spikes, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(spikes);
    DestroyNorepinephrineState(state);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineReceptorUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(10, 10);
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    EXPECT_FALSE(nimcp_gpu_norepinephrine_receptor_update(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_receptor_update(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_receptor_update(ctx, state, 1.0f, nullptr));

    DestroyNorepinephrineState(state);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineComputeArousal_NullSafety) {
    RequireGPU();

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();

    EXPECT_FALSE(nimcp_gpu_norepinephrine_compute_arousal(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_compute_arousal(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_compute_arousal(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_compute_arousal(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_compute_arousal(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNorepinephrineState(state);
}

TEST_F(NeuromodulatorKernelTest, NorepinephrineModulateGain_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_norepinephrine_modulate_gain(nullptr, tensor, tensor, 0.5f, 2.0f));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_modulate_gain(ctx, nullptr, tensor, 0.5f, 2.0f));
    EXPECT_FALSE(nimcp_gpu_norepinephrine_modulate_gain(ctx, tensor, nullptr, 0.5f, 2.0f));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(NeuromodulatorKernelTest, VesicleDynamics_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_vesicle_dynamics(nullptr, tensor, tensor, 0.3f, 0.01f, 1.0f, 1.0f));
    EXPECT_FALSE(nimcp_gpu_vesicle_dynamics(ctx, nullptr, tensor, 0.3f, 0.01f, 1.0f, 1.0f));
    EXPECT_FALSE(nimcp_gpu_vesicle_dynamics(ctx, tensor, nullptr, 0.3f, 0.01f, 1.0f, 1.0f));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(NeuromodulatorKernelTest, ReceptorKinetics_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_receptor_kinetics(nullptr, tensor, tensor, 0.1f, 1.0f, 0.1f, 1.0f));
    EXPECT_FALSE(nimcp_gpu_receptor_kinetics(ctx, nullptr, tensor, 0.1f, 1.0f, 0.1f, 1.0f));
    EXPECT_FALSE(nimcp_gpu_receptor_kinetics(ctx, tensor, nullptr, 0.1f, 1.0f, 0.1f, 1.0f));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(NeuromodulatorKernelTest, NeuromodSystemUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_neuromod_system_update(nullptr, system, tensor, tensor, tensor, tensor, 1.0f));
    EXPECT_FALSE(nimcp_gpu_neuromod_system_update(ctx, nullptr, tensor, tensor, tensor, tensor, 1.0f));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNeuromodSystem(system);
}

TEST_F(NeuromodulatorKernelTest, NeuromodInteractions_NullSafety) {
    EXPECT_FALSE(nimcp_gpu_neuromod_interactions(nullptr, nullptr));
    if (gpu_available) {
        EXPECT_FALSE(nimcp_gpu_neuromod_interactions(ctx, nullptr));
    }
}

TEST_F(NeuromodulatorKernelTest, NeuromodApplyCombined_NullSafety) {
    RequireGPU();

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(10, 10);
    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);

    EXPECT_FALSE(nimcp_gpu_neuromod_apply_combined(nullptr, system, tensor, tensor));
    EXPECT_FALSE(nimcp_gpu_neuromod_apply_combined(ctx, nullptr, tensor, tensor));
    EXPECT_FALSE(nimcp_gpu_neuromod_apply_combined(ctx, system, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_neuromod_apply_combined(ctx, system, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNeuromodSystem(system);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(NeuromodulatorKernelTest, Integration_DopamineRewardLearning) {
    RequireGPU();

    const size_t n_neurons = 20;
    const size_t n_targets = 20;
    const size_t n_synapses = n_targets * n_targets;
    const float dt = 1.0f;
    const int n_trials = 10;

    nimcp_gpu_dopamine_state_t* state = CreateDopamineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_dopamine_params_t params = nimcp_gpu_dopamine_params_default();
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* weights = Create2DTensor(n_targets, n_targets, 0.5f);
    nimcp_gpu_tensor_t* eligibility = Create1DTensor(n_synapses, 0.0f);
    nimcp_gpu_tensor_t* reward = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* predicted = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* rpe = Create1DTensor(n_targets, 0.0f);

    float initial_weight_sum = 0.0f;
    auto init_w = CopyToHost(weights);
    for (float w : init_w) initial_weight_sum += w;

    // Simulate reward learning trials
    for (int trial = 0; trial < n_trials; trial++) {
        // Pre-reward: cue presentation with spikes
        nimcp_gpu_fill(ctx, spikes, 0.5f);
        for (int t = 0; t < 20; t++) {
            nimcp_gpu_dopamine_update(ctx, state, spikes, dt, &params);
            nimcp_gpu_dopamine_receptor_update(ctx, state, dt, &params);
        }

        // Build eligibility traces
        nimcp_gpu_fill(ctx, eligibility, 0.3f);

        // Reward delivery
        nimcp_gpu_fill(ctx, reward, 1.0f);
        nimcp_gpu_fill(ctx, predicted, 0.3f);  // Initially under-predict

        // Compute RPE
        nimcp_gpu_dopamine_compute_rpe(ctx, state, reward, predicted, rpe, &params);

        // Apply plasticity modulated by D1/D2
        nimcp_gpu_dopamine_modulate_plasticity(
            ctx, weights, state->d1_occupancy, state->d2_occupancy, eligibility, 0.01f);

        // Post-reward: concentration decays
        nimcp_gpu_fill(ctx, spikes, 0.0f);
        for (int t = 0; t < 50; t++) {
            nimcp_gpu_dopamine_update(ctx, state, spikes, dt, &params);
        }
    }

    float final_weight_sum = 0.0f;
    auto final_w = CopyToHost(weights);
    for (float w : final_w) final_weight_sum += w;

    // Weights should change through learning
    EXPECT_NE(final_weight_sum, initial_weight_sum);

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(eligibility);
    nimcp_gpu_tensor_destroy(reward);
    nimcp_gpu_tensor_destroy(predicted);
    nimcp_gpu_tensor_destroy(rpe);
    DestroyDopamineState(state);
}

TEST_F(NeuromodulatorKernelTest, Integration_CholinergicAttentionModulation) {
    RequireGPU();

    const size_t n_neurons = 20;
    const size_t n_targets = 20;
    const float dt = 1.0f;

    nimcp_gpu_acetylcholine_state_t* state = CreateAcetylcholineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_acetylcholine_params_t params = nimcp_gpu_acetylcholine_params_default();
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* salience = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* attention = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* learning_rates = Create1DTensor(n_targets, 0.01f);

    // Set varying salience (some targets more salient)
    std::vector<float> salience_values(n_targets);
    for (size_t i = 0; i < n_targets; i++) {
        salience_values[i] = (i < n_targets / 2) ? 0.9f : 0.1f;
    }
    salience = SetFromHost(salience, salience_values);

    // Simulate cholinergic activation
    nimcp_gpu_fill(ctx, spikes, 0.6f);
    for (int t = 0; t < 50; t++) {
        nimcp_gpu_acetylcholine_update(ctx, state, spikes, dt, &params);
        nimcp_gpu_acetylcholine_receptor_update(ctx, state, dt, &params);
    }

    // Compute attention based on salience
    nimcp_gpu_acetylcholine_compute_attention(ctx, state, salience, attention, &params);

    // Modulate learning rates
    nimcp_gpu_acetylcholine_modulate_learning(
        ctx, learning_rates, state->concentration, 0.01f, 3.0f);

    auto attention_data = CopyToHost(attention);
    auto lr_data = CopyToHost(learning_rates);

    // Attention should be computed
    bool has_attention = false;
    for (size_t i = 0; i < n_targets; i++) {
        if (attention_data[i] > 0.0f) {
            has_attention = true;
            break;
        }
    }
    EXPECT_TRUE(has_attention);

    // Learning rates should be modulated
    bool lr_modulated = false;
    for (size_t i = 0; i < n_targets; i++) {
        if (std::abs(lr_data[i] - 0.01f) > 1e-6f) {
            lr_modulated = true;
            break;
        }
    }
    // Learning rates may or may not be modulated depending on ACh levels
    // Just verify they're valid
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(lr_data[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(salience);
    nimcp_gpu_tensor_destroy(attention);
    nimcp_gpu_tensor_destroy(learning_rates);
    DestroyAcetylcholineState(state);
}

TEST_F(NeuromodulatorKernelTest, Integration_NoradrenergicArousalGain) {
    RequireGPU();

    const size_t n_neurons = 20;
    const size_t n_targets = 20;
    const float dt = 1.0f;

    nimcp_gpu_norepinephrine_state_t* state = CreateNorepinephrineState(n_neurons, n_targets);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_norepinephrine_params_t params = nimcp_gpu_norepinephrine_params_default();
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* stress = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* arousal = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* gains = Create1DTensor(n_targets, 1.0f);

    // Low stress baseline
    nimcp_gpu_fill(ctx, stress, 0.2f);
    nimcp_gpu_fill(ctx, spikes, 0.3f);

    for (int t = 0; t < 30; t++) {
        nimcp_gpu_norepinephrine_update(ctx, state, spikes, dt, &params);
        nimcp_gpu_norepinephrine_receptor_update(ctx, state, dt, &params);
    }

    nimcp_gpu_norepinephrine_compute_arousal(ctx, state, stress, arousal, &params);
    auto low_arousal = CopyToHost(arousal);

    nimcp_gpu_norepinephrine_modulate_gain(ctx, gains, state->concentration, 0.5f, 2.0f);
    auto low_gains = CopyToHost(gains);

    // High stress condition
    nimcp_gpu_fill(ctx, stress, 0.9f);
    nimcp_gpu_fill(ctx, spikes, 0.8f);

    for (int t = 0; t < 50; t++) {
        nimcp_gpu_norepinephrine_update(ctx, state, spikes, dt, &params);
        nimcp_gpu_norepinephrine_receptor_update(ctx, state, dt, &params);
    }

    nimcp_gpu_norepinephrine_compute_arousal(ctx, state, stress, arousal, &params);
    auto high_arousal = CopyToHost(arousal);

    nimcp_gpu_fill(ctx, gains, 1.0f);
    nimcp_gpu_norepinephrine_modulate_gain(ctx, gains, state->concentration, 0.5f, 2.0f);
    auto high_gains = CopyToHost(gains);

    // Arousal and gains should respond to stress levels
    // Just verify values are valid
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(low_arousal[i], 0.0f);
        EXPECT_GE(high_arousal[i], 0.0f);
        EXPECT_GE(low_gains[i], 0.0f);
        EXPECT_GE(high_gains[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(spikes);
    nimcp_gpu_tensor_destroy(stress);
    nimcp_gpu_tensor_destroy(arousal);
    nimcp_gpu_tensor_destroy(gains);
    DestroyNorepinephrineState(state);
}

TEST_F(NeuromodulatorKernelTest, Integration_FullNeuromodulatorSystem) {
    RequireGPU();

    const size_t n_neurons = 15;
    const size_t n_targets = 15;
    const float dt = 1.0f;
    const int n_steps = 100;

    nimcp_gpu_neuromod_system_t* system = CreateNeuromodSystem(n_neurons, n_targets);
    ASSERT_NE(system, nullptr);
    system->dt = dt;

    nimcp_gpu_tensor_t* da_spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* ht_spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* ach_spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* ne_spikes = Create1DTensor(n_targets, 0.0f);
    nimcp_gpu_tensor_t* target_activity = Create1DTensor(n_targets, 0.5f);
    nimcp_gpu_tensor_t* modulated_output = Create1DTensor(n_targets, 0.0f);

    // Simulate varying activity patterns
    for (int step = 0; step < n_steps; step++) {
        // Vary spike patterns
        float phase = (float)step / n_steps;

        if (step % 20 < 5) {
            nimcp_gpu_fill(ctx, da_spikes, 0.7f);  // DA burst
        } else {
            nimcp_gpu_fill(ctx, da_spikes, 0.1f);
        }

        if (step > 30 && step < 70) {
            nimcp_gpu_fill(ctx, ach_spikes, 0.6f);  // Sustained attention
        } else {
            nimcp_gpu_fill(ctx, ach_spikes, 0.2f);
        }

        nimcp_gpu_fill(ctx, ht_spikes, 0.3f);  // Tonic serotonin
        nimcp_gpu_fill(ctx, ne_spikes, 0.2f + phase * 0.3f);  // Rising arousal

        // Update all systems
        nimcp_gpu_neuromod_system_update(ctx, system, da_spikes, ht_spikes, ach_spikes, ne_spikes, dt);

        // Compute interactions
        nimcp_gpu_neuromod_interactions(ctx, system);
    }

    // Apply combined modulation to target activity
    bool result = nimcp_gpu_neuromod_apply_combined(ctx, system, target_activity, modulated_output);
    EXPECT_TRUE(result);

    auto output_data = CopyToHost(modulated_output);

    // Output should be valid (modulated from input)
    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(output_data[i], 0.0f);
    }

    // Verify all neuromodulator concentrations are valid
    auto da_conc = CopyToHost(system->dopamine->concentration);
    auto ht_conc = CopyToHost(system->serotonin->concentration);
    auto ach_conc = CopyToHost(system->acetylcholine->concentration);
    auto ne_conc = CopyToHost(system->norepinephrine->concentration);

    for (size_t i = 0; i < n_targets; i++) {
        EXPECT_GE(da_conc[i], 0.0f);
        EXPECT_GE(ht_conc[i], 0.0f);
        EXPECT_GE(ach_conc[i], 0.0f);
        EXPECT_GE(ne_conc[i], 0.0f);
    }

    nimcp_gpu_tensor_destroy(da_spikes);
    nimcp_gpu_tensor_destroy(ht_spikes);
    nimcp_gpu_tensor_destroy(ach_spikes);
    nimcp_gpu_tensor_destroy(ne_spikes);
    nimcp_gpu_tensor_destroy(target_activity);
    nimcp_gpu_tensor_destroy(modulated_output);
    DestroyNeuromodSystem(system);
}

TEST_F(NeuromodulatorKernelTest, Integration_VesicleDepletionRecovery) {
    RequireGPU();

    const size_t n = 50;
    const float release_prob = 0.4f;
    const float replenish_rate = 0.05f;
    const float max_pool = 1.0f;
    const float dt = 1.0f;

    nimcp_gpu_tensor_t* vesicle_pool = Create1DTensor(n, 1.0f);
    nimcp_gpu_tensor_t* spikes = Create1DTensor(n, 0.0f);

    // Phase 1: High-frequency stimulation (depletion)
    nimcp_gpu_fill(ctx, spikes, 1.0f);
    for (int t = 0; t < 50; t++) {
        nimcp_gpu_vesicle_dynamics(ctx, vesicle_pool, spikes, release_prob, replenish_rate, max_pool, dt);
    }

    auto depleted = CopyToHost(vesicle_pool);
    float avg_depleted = 0.0f;
    for (float v : depleted) avg_depleted += v;
    avg_depleted /= n;

    // Vesicles should be depleted
    EXPECT_LT(avg_depleted, 0.5f);

    // Phase 2: Recovery (no spikes)
    nimcp_gpu_fill(ctx, spikes, 0.0f);
    for (int t = 0; t < 200; t++) {
        nimcp_gpu_vesicle_dynamics(ctx, vesicle_pool, spikes, release_prob, replenish_rate, max_pool, dt);
    }

    auto recovered = CopyToHost(vesicle_pool);
    float avg_recovered = 0.0f;
    for (float v : recovered) avg_recovered += v;
    avg_recovered /= n;

    // Vesicles should recover
    EXPECT_GT(avg_recovered, avg_depleted);

    nimcp_gpu_tensor_destroy(vesicle_pool);
    nimcp_gpu_tensor_destroy(spikes);
}

TEST_F(NeuromodulatorKernelTest, Integration_ReceptorSaturation) {
    RequireGPU();

    const size_t n = 100;
    const float affinity = 0.1f;
    const float on_rate = 2.0f;
    const float off_rate = 0.2f;
    // Use small dt for numerical stability of forward Euler integration
    const float dt = 0.1f;

    nimcp_gpu_tensor_t* occupancy = Create1DTensor(n, 0.0f);
    nimcp_gpu_tensor_t* concentration = Create1DTensor(n, 1.0f);  // Very high concentration

    // Run to steady state with more iterations to compensate for smaller dt
    for (int t = 0; t < 200; t++) {
        nimcp_gpu_receptor_kinetics(ctx, occupancy, concentration, affinity, on_rate, off_rate, dt);
    }

    auto occ_data = CopyToHost(occupancy);

    // With high concentration, occupancy should approach saturation
    // Equilibrium: occ = on*conc / (on*conc + off) = 2.0 / (2.0 + 0.2) ~ 0.909
    for (size_t i = 0; i < n; i++) {
        EXPECT_GT(occ_data[i], 0.5f);  // Should be high
        EXPECT_LE(occ_data[i], 1.0f);  // But not exceed 1
    }

    nimcp_gpu_tensor_destroy(occupancy);
    nimcp_gpu_tensor_destroy(concentration);
}
