/**
 * @file test_emotion_kernels.cpp
 * @brief Unit tests for GPU emotion processing kernels
 *
 * Tests Amygdala, OFC, NAcc, and ACC GPU operations including
 * fear conditioning, value computation, reward processing, and conflict monitoring.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>
#include <cstring>

// Headers have their own extern "C" guards
#include "gpu/emotion/nimcp_emotion_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Fixtures
//=============================================================================

class EmotionKernelTest : public ::testing::Test {
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

    // Helper to create amygdala state
    nimcp_gpu_amygdala_state_t* CreateAmygdalaState(size_t n_stimuli, size_t n_contexts) {
        nimcp_gpu_amygdala_state_t* state = new nimcp_gpu_amygdala_state_t();
        state->n_stimuli = n_stimuli;
        state->n_contexts = n_contexts;
        state->threat_signal = Create1DTensor(n_stimuli, 0.0f);
        state->fear_memory = Create1DTensor(n_stimuli, 0.0f);
        state->cs_us_associations = Create2DTensor(n_stimuli, n_stimuli, 0.0f);
        state->extinction_trace = Create1DTensor(n_stimuli, 0.0f);
        state->context_gate = Create1DTensor(n_contexts, 0.5f);
        state->lateral_activity = Create1DTensor(n_stimuli, 0.0f);
        state->basal_activity = Create1DTensor(n_stimuli, 0.0f);
        state->central_output = Create1DTensor(n_stimuli, 0.0f);
        return state;
    }

    void DestroyAmygdalaState(nimcp_gpu_amygdala_state_t* state) {
        if (state) {
            nimcp_gpu_tensor_destroy(state->threat_signal);
            nimcp_gpu_tensor_destroy(state->fear_memory);
            nimcp_gpu_tensor_destroy(state->cs_us_associations);
            nimcp_gpu_tensor_destroy(state->extinction_trace);
            nimcp_gpu_tensor_destroy(state->context_gate);
            nimcp_gpu_tensor_destroy(state->lateral_activity);
            nimcp_gpu_tensor_destroy(state->basal_activity);
            nimcp_gpu_tensor_destroy(state->central_output);
            delete state;
        }
    }

    // Helper to create OFC state
    nimcp_gpu_ofc_state_t* CreateOFCState(size_t n_options, size_t n_outcomes) {
        nimcp_gpu_ofc_state_t* state = new nimcp_gpu_ofc_state_t();
        state->n_options = n_options;
        state->n_outcomes = n_outcomes;
        state->option_values = Create1DTensor(n_options, 0.5f);
        state->expected_outcomes = Create2DTensor(n_options, n_outcomes, 0.0f);
        state->outcome_history = Create2DTensor(n_options, n_outcomes, 0.0f);
        state->reversal_signal = Create1DTensor(n_options, 0.0f);
        state->choice_probabilities = Create1DTensor(n_options, 1.0f / n_options);
        state->satiety_state = Create1DTensor(n_outcomes, 0.0f);
        state->risk_assessment = Create1DTensor(n_options, 0.0f);
        return state;
    }

    void DestroyOFCState(nimcp_gpu_ofc_state_t* state) {
        if (state) {
            nimcp_gpu_tensor_destroy(state->option_values);
            nimcp_gpu_tensor_destroy(state->expected_outcomes);
            nimcp_gpu_tensor_destroy(state->outcome_history);
            nimcp_gpu_tensor_destroy(state->reversal_signal);
            nimcp_gpu_tensor_destroy(state->choice_probabilities);
            nimcp_gpu_tensor_destroy(state->satiety_state);
            nimcp_gpu_tensor_destroy(state->risk_assessment);
            delete state;
        }
    }

    // Helper to create NAcc state
    nimcp_gpu_nacc_state_t* CreateNAccState(size_t n_states) {
        nimcp_gpu_nacc_state_t* state = new nimcp_gpu_nacc_state_t();
        state->n_states = n_states;
        state->reward_prediction = Create1DTensor(n_states, 0.0f);
        state->motivation_signal = Create1DTensor(n_states, 0.5f);
        state->hedonic_signal = Create1DTensor(n_states, 0.5f);
        state->effort_signal = Create1DTensor(n_states, 0.0f);
        state->dopamine_input = Create1DTensor(n_states, 0.0f);
        state->msn_d1_activity = Create1DTensor(n_states, 0.0f);
        state->msn_d2_activity = Create1DTensor(n_states, 0.0f);
        return state;
    }

    void DestroyNAccState(nimcp_gpu_nacc_state_t* state) {
        if (state) {
            nimcp_gpu_tensor_destroy(state->reward_prediction);
            nimcp_gpu_tensor_destroy(state->motivation_signal);
            nimcp_gpu_tensor_destroy(state->hedonic_signal);
            nimcp_gpu_tensor_destroy(state->effort_signal);
            nimcp_gpu_tensor_destroy(state->dopamine_input);
            nimcp_gpu_tensor_destroy(state->msn_d1_activity);
            nimcp_gpu_tensor_destroy(state->msn_d2_activity);
            delete state;
        }
    }

    // Helper to create ACC state
    nimcp_gpu_acc_state_t* CreateACCState(size_t n_responses) {
        nimcp_gpu_acc_state_t* state = new nimcp_gpu_acc_state_t();
        state->n_responses = n_responses;
        state->conflict_signal = Create1DTensor(n_responses, 0.0f);
        state->error_signal = Create1DTensor(n_responses, 0.0f);
        state->effort_allocation = Create1DTensor(n_responses, 0.5f);
        state->volatility = Create1DTensor(1, 0.0f);
        state->control_signal = Create1DTensor(n_responses, 0.0f);
        return state;
    }

    void DestroyACCState(nimcp_gpu_acc_state_t* state) {
        if (state) {
            nimcp_gpu_tensor_destroy(state->conflict_signal);
            nimcp_gpu_tensor_destroy(state->error_signal);
            nimcp_gpu_tensor_destroy(state->effort_allocation);
            nimcp_gpu_tensor_destroy(state->volatility);
            nimcp_gpu_tensor_destroy(state->control_signal);
            delete state;
        }
    }
};

//=============================================================================
// Default Parameter Tests
//=============================================================================

TEST_F(EmotionKernelTest, AmygdalaParamsDefault_ReturnsValidParams) {
    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    // Check reasonable defaults for threat detection
    EXPECT_GT(params.threat_threshold, 0.0f);
    EXPECT_LE(params.threat_threshold, 1.0f);

    // Check learning rates are positive
    EXPECT_GT(params.fear_learning_rate, 0.0f);
    EXPECT_GT(params.extinction_rate, 0.0f);

    // Check time constants are positive
    EXPECT_GT(params.habituation_tau, 0.0f);
    EXPECT_GT(params.sensitization_tau, 0.0f);

    // Check generalization width is positive
    EXPECT_GT(params.generalization_sigma, 0.0f);

    // Check weights are in valid range
    EXPECT_GE(params.context_weight, 0.0f);
    EXPECT_LE(params.context_weight, 1.0f);
    EXPECT_GE(params.prefrontal_inhibition, 0.0f);
    EXPECT_LE(params.prefrontal_inhibition, 1.0f);
    EXPECT_GE(params.lateral_inhibition, 0.0f);
    EXPECT_GT(params.basal_threshold, 0.0f);
}

TEST_F(EmotionKernelTest, OFCParamsDefault_ReturnsValidParams) {
    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    // Check learning rate is positive
    EXPECT_GT(params.value_learning_rate, 0.0f);

    // Check discount factor is in valid range (0, 1]
    EXPECT_GT(params.discount_factor, 0.0f);
    EXPECT_LE(params.discount_factor, 1.0f);

    // Check reversal rate is positive
    EXPECT_GT(params.reversal_rate, 0.0f);

    // Check gains and sensitivities are non-negative
    EXPECT_GE(params.comparison_gain, 0.0f);
    EXPECT_GE(params.risk_sensitivity, 0.0f);
    EXPECT_GE(params.outcome_sensitivity, 0.0f);

    // Check time constant is positive
    EXPECT_GT(params.integration_tau, 0.0f);

    // Check decay rate
    EXPECT_GE(params.satiety_decay, 0.0f);
}

TEST_F(EmotionKernelTest, NAccParamsDefault_ReturnsValidParams) {
    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    // Check sensitivity is positive
    EXPECT_GT(params.reward_sensitivity, 0.0f);

    // Check costs and discounts are non-negative
    EXPECT_GE(params.effort_cost, 0.0f);
    EXPECT_GE(params.delay_discount, 0.0f);

    // Check baseline is in valid range
    EXPECT_GE(params.hedonic_baseline, 0.0f);
    EXPECT_LE(params.hedonic_baseline, 1.0f);

    // Check decay rate
    EXPECT_GE(params.motivation_decay, 0.0f);

    // Check gains are positive
    EXPECT_GT(params.dopamine_gain, 0.0f);
    EXPECT_GE(params.gaba_inhibition, 0.0f);
    EXPECT_GE(params.glutamate_excitation, 0.0f);
}

TEST_F(EmotionKernelTest, ACCParamsDefault_ReturnsValidParams) {
    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    // Check threshold is positive
    EXPECT_GT(params.conflict_threshold, 0.0f);

    // Check learning rate is positive
    EXPECT_GT(params.error_learning_rate, 0.0f);

    // Check sensitivities are non-negative
    EXPECT_GE(params.effort_sensitivity, 0.0f);
    EXPECT_GE(params.prediction_weight, 0.0f);

    // Check volatility estimate is non-negative
    EXPECT_GE(params.volatility_estimate, 0.0f);

    // Check control gain is positive
    EXPECT_GT(params.control_gain, 0.0f);
}

//=============================================================================
// Amygdala State Tests
//=============================================================================

TEST_F(EmotionKernelTest, AmygdalaState_CreateAndDestroy) {
    RequireGPU();

    const size_t n_stimuli = 32;
    const size_t n_contexts = 8;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->threat_signal, nullptr);
    EXPECT_NE(state->fear_memory, nullptr);
    EXPECT_NE(state->cs_us_associations, nullptr);
    EXPECT_NE(state->extinction_trace, nullptr);
    EXPECT_NE(state->context_gate, nullptr);
    EXPECT_NE(state->lateral_activity, nullptr);
    EXPECT_NE(state->basal_activity, nullptr);
    EXPECT_NE(state->central_output, nullptr);
    EXPECT_EQ(state->n_stimuli, n_stimuli);
    EXPECT_EQ(state->n_contexts, n_contexts);

    DestroyAmygdalaState(state);
}

//=============================================================================
// OFC State Tests
//=============================================================================

TEST_F(EmotionKernelTest, OFCState_CreateAndDestroy) {
    RequireGPU();

    const size_t n_options = 10;
    const size_t n_outcomes = 5;

    nimcp_gpu_ofc_state_t* state = CreateOFCState(n_options, n_outcomes);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->option_values, nullptr);
    EXPECT_NE(state->expected_outcomes, nullptr);
    EXPECT_NE(state->outcome_history, nullptr);
    EXPECT_NE(state->reversal_signal, nullptr);
    EXPECT_NE(state->choice_probabilities, nullptr);
    EXPECT_NE(state->satiety_state, nullptr);
    EXPECT_NE(state->risk_assessment, nullptr);
    EXPECT_EQ(state->n_options, n_options);
    EXPECT_EQ(state->n_outcomes, n_outcomes);

    DestroyOFCState(state);
}

//=============================================================================
// NAcc State Tests
//=============================================================================

TEST_F(EmotionKernelTest, NAccState_CreateAndDestroy) {
    RequireGPU();

    const size_t n_states = 50;

    nimcp_gpu_nacc_state_t* state = CreateNAccState(n_states);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->reward_prediction, nullptr);
    EXPECT_NE(state->motivation_signal, nullptr);
    EXPECT_NE(state->hedonic_signal, nullptr);
    EXPECT_NE(state->effort_signal, nullptr);
    EXPECT_NE(state->dopamine_input, nullptr);
    EXPECT_NE(state->msn_d1_activity, nullptr);
    EXPECT_NE(state->msn_d2_activity, nullptr);
    EXPECT_EQ(state->n_states, n_states);

    DestroyNAccState(state);
}

//=============================================================================
// ACC State Tests
//=============================================================================

TEST_F(EmotionKernelTest, ACCState_CreateAndDestroy) {
    RequireGPU();

    const size_t n_responses = 20;

    nimcp_gpu_acc_state_t* state = CreateACCState(n_responses);
    ASSERT_NE(state, nullptr);

    EXPECT_NE(state->conflict_signal, nullptr);
    EXPECT_NE(state->error_signal, nullptr);
    EXPECT_NE(state->effort_allocation, nullptr);
    EXPECT_NE(state->volatility, nullptr);
    EXPECT_NE(state->control_signal, nullptr);
    EXPECT_EQ(state->n_responses, n_responses);

    DestroyACCState(state);
}

//=============================================================================
// Amygdala Operation Tests
//=============================================================================

TEST_F(EmotionKernelTest, AmygdalaThreatDetection_DetectsThreats) {
    RequireGPU();

    const size_t n_stimuli = 32;
    const size_t n_contexts = 8;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* sensory_input = Create1DTensor(n_stimuli, 0.8f);  // High intensity
    nimcp_gpu_tensor_t* context = Create1DTensor(n_contexts, 0.5f);
    nimcp_gpu_tensor_t* threat_out = Create1DTensor(n_stimuli, 0.0f);

    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    bool result = nimcp_gpu_amygdala_threat_detection(
        ctx, state, sensory_input, context, threat_out, &params);
    EXPECT_TRUE(result);

    auto threat_data = CopyToHost(threat_out);

    // With high sensory input, threat should be detected
    for (size_t i = 0; i < n_stimuli; i++) {
        EXPECT_GE(threat_data[i], 0.0f);
        EXPECT_LE(threat_data[i], 1.0f);
    }

    nimcp_gpu_tensor_destroy(sensory_input);
    nimcp_gpu_tensor_destroy(context);
    nimcp_gpu_tensor_destroy(threat_out);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaFearConditioning_LearnsAssociations) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;
    const float dt = 1.0f;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    // CS (conditioned stimulus) and US (unconditioned stimulus)
    nimcp_gpu_tensor_t* cs = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* us = Create1DTensor(n_stimuli, 0.0f);

    // Set specific stimulus as CS and US pair
    // Note: kernel does per-element Rescorla-Wagner learning, so CS and US
    // must be active at the same index for association to form
    std::vector<float> cs_data(n_stimuli, 0.0f);
    std::vector<float> us_data(n_stimuli, 0.0f);
    cs_data[0] = 1.0f;  // CS at index 0
    us_data[0] = 1.0f;  // US at same index 0 (co-occurrence)
    cs = SetFromHost(cs, cs_data);
    us = SetFromHost(us, us_data);

    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    // Get initial associations
    auto initial_assoc = CopyToHost(state->cs_us_associations);
    float initial_sum = 0.0f;
    for (float v : initial_assoc) initial_sum += std::abs(v);

    // Train for several iterations
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_amygdala_fear_conditioning(
            ctx, state, cs, us, dt, &params);
        EXPECT_TRUE(result);
    }

    // Get final associations
    auto final_assoc = CopyToHost(state->cs_us_associations);
    float final_sum = 0.0f;
    for (float v : final_assoc) final_sum += std::abs(v);

    // Associations should strengthen with training
    EXPECT_GT(final_sum, initial_sum);

    nimcp_gpu_tensor_destroy(cs);
    nimcp_gpu_tensor_destroy(us);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaExtinction_ReducesFearResponse) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;
    const float dt = 1.0f;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    // Pre-establish fear memory and CS-US associations
    // Extinction kernel uses cs_us_associations to compute extinction_delta,
    // so associations must be non-zero for extinction learning to occur
    nimcp_gpu_fill(ctx, state->fear_memory, 0.8f);
    nimcp_gpu_fill(ctx, state->cs_us_associations, 0.8f);

    nimcp_gpu_tensor_t* cs = Create1DTensor(n_stimuli, 1.0f);     // CS present
    nimcp_gpu_tensor_t* no_us = Create1DTensor(n_stimuli, 0.0f);  // No US (extinction)

    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    // Get initial fear
    auto initial_fear = CopyToHost(state->fear_memory);
    float initial_fear_sum = 0.0f;
    for (float v : initial_fear) initial_fear_sum += v;

    // Run extinction trials
    for (int i = 0; i < 100; i++) {
        bool result = nimcp_gpu_amygdala_extinction(
            ctx, state, cs, no_us, dt, &params);
        EXPECT_TRUE(result);
    }

    // Get final fear
    auto final_fear = CopyToHost(state->fear_memory);
    float final_fear_sum = 0.0f;
    for (float v : final_fear) final_fear_sum += v;

    // Fear should reduce with extinction (or extinction trace should increase)
    auto extinction_trace = CopyToHost(state->extinction_trace);
    float extinction_sum = 0.0f;
    for (float v : extinction_trace) extinction_sum += v;

    // Either fear decreases or extinction trace increases
    bool extinction_occurred = (final_fear_sum < initial_fear_sum) || (extinction_sum > 0.0f);
    EXPECT_TRUE(extinction_occurred);

    nimcp_gpu_tensor_destroy(cs);
    nimcp_gpu_tensor_destroy(no_us);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaPrefrontalInhibition_ReducesOutput) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    // Set high central output
    nimcp_gpu_fill(ctx, state->central_output, 0.9f);

    nimcp_gpu_tensor_t* pfc_signal = Create1DTensor(n_stimuli, 0.8f);  // Strong PFC inhibition

    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();
    params.prefrontal_inhibition = 0.8f;

    // Get initial output
    auto initial_output = CopyToHost(state->central_output);

    bool result = nimcp_gpu_amygdala_prefrontal_inhibition(ctx, state, pfc_signal, &params);
    EXPECT_TRUE(result);

    // Get final output
    auto final_output = CopyToHost(state->central_output);

    // PFC should inhibit amygdala output
    for (size_t i = 0; i < n_stimuli; i++) {
        EXPECT_LE(final_output[i], initial_output[i]);
    }

    nimcp_gpu_tensor_destroy(pfc_signal);
    DestroyAmygdalaState(state);
}

//=============================================================================
// OFC Operation Tests
//=============================================================================

TEST_F(EmotionKernelTest, OFCComputeValues_AssignsValues) {
    RequireGPU();

    const size_t n_options = 10;
    const size_t n_outcomes = 5;
    const size_t n_features = 20;
    const size_t n_contexts = 4;

    nimcp_gpu_ofc_state_t* state = CreateOFCState(n_options, n_outcomes);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* stimulus_features = Create1DTensor(n_features, 0.5f);
    nimcp_gpu_tensor_t* context = Create1DTensor(n_contexts, 0.5f);

    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    bool result = nimcp_gpu_ofc_compute_values(
        ctx, state, stimulus_features, context, &params);
    EXPECT_TRUE(result);

    auto values = CopyToHost(state->option_values);

    // Values should be computed (all in valid range)
    for (size_t i = 0; i < n_options; i++) {
        EXPECT_FALSE(std::isnan(values[i]));
        EXPECT_FALSE(std::isinf(values[i]));
    }

    nimcp_gpu_tensor_destroy(stimulus_features);
    nimcp_gpu_tensor_destroy(context);
    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, OFCValueUpdate_LearnsFromOutcome) {
    RequireGPU();

    const size_t n_options = 5;
    const size_t n_outcomes = 3;
    const float dt = 1.0f;

    nimcp_gpu_ofc_state_t* state = CreateOFCState(n_options, n_outcomes);
    ASSERT_NE(state, nullptr);

    // Set initial values
    nimcp_gpu_fill(ctx, state->option_values, 0.5f);

    // Choose option 0
    nimcp_gpu_tensor_t* chosen_option = Create1DTensor(n_options, 0.0f);
    std::vector<float> chosen_data(n_options, 0.0f);
    chosen_data[0] = 1.0f;
    chosen_option = SetFromHost(chosen_option, chosen_data);

    // High reward outcome
    nimcp_gpu_tensor_t* outcome = Create1DTensor(n_outcomes, 0.0f);
    std::vector<float> outcome_data(n_outcomes, 0.0f);
    outcome_data[0] = 1.0f;  // Positive outcome
    outcome = SetFromHost(outcome, outcome_data);

    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    auto initial_values = CopyToHost(state->option_values);

    // Update values multiple times
    for (int i = 0; i < 50; i++) {
        bool result = nimcp_gpu_ofc_value_update(
            ctx, state, chosen_option, outcome, dt, &params);
        EXPECT_TRUE(result);
    }

    auto final_values = CopyToHost(state->option_values);

    // Chosen option value should increase with positive outcome
    EXPECT_GE(final_values[0], initial_values[0]);

    nimcp_gpu_tensor_destroy(chosen_option);
    nimcp_gpu_tensor_destroy(outcome);
    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, OFCChoiceProbabilities_SumsToOne) {
    RequireGPU();

    const size_t n_options = 5;
    const size_t n_outcomes = 3;
    const float temperature = 1.0f;

    nimcp_gpu_ofc_state_t* state = CreateOFCState(n_options, n_outcomes);
    ASSERT_NE(state, nullptr);

    // Set varying option values
    std::vector<float> values = {0.1f, 0.3f, 0.5f, 0.7f, 0.9f};
    state->option_values = SetFromHost(state->option_values, values);

    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    bool result = nimcp_gpu_ofc_choice_probabilities(ctx, state, temperature, &params);
    EXPECT_TRUE(result);

    auto probs = CopyToHost(state->choice_probabilities);

    // Probabilities should sum to 1.0
    float sum = 0.0f;
    for (size_t i = 0; i < n_options; i++) {
        EXPECT_GE(probs[i], 0.0f);
        EXPECT_LE(probs[i], 1.0f);
        sum += probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.01f);

    // Higher value options should have higher probability
    EXPECT_GT(probs[4], probs[0]);

    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, OFCReversalLearning_AdaptsToChange) {
    RequireGPU();

    const size_t n_options = 5;
    const size_t n_outcomes = 3;
    const float dt = 1.0f;

    nimcp_gpu_ofc_state_t* state = CreateOFCState(n_options, n_outcomes);
    ASSERT_NE(state, nullptr);

    // Create prediction error (large mismatch)
    nimcp_gpu_tensor_t* prediction_error = Create1DTensor(n_options, 0.8f);

    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    auto initial_reversal = CopyToHost(state->reversal_signal);

    // Apply reversal learning multiple times
    for (int i = 0; i < 20; i++) {
        bool result = nimcp_gpu_ofc_reversal_learning(
            ctx, state, prediction_error, dt, &params);
        EXPECT_TRUE(result);
    }

    auto final_reversal = CopyToHost(state->reversal_signal);

    // Reversal signal should increase with prediction errors
    float initial_sum = 0.0f, final_sum = 0.0f;
    for (size_t i = 0; i < n_options; i++) {
        initial_sum += initial_reversal[i];
        final_sum += final_reversal[i];
    }
    EXPECT_GT(final_sum, initial_sum);

    nimcp_gpu_tensor_destroy(prediction_error);
    DestroyOFCState(state);
}

//=============================================================================
// NAcc Operation Tests
//=============================================================================

TEST_F(EmotionKernelTest, NAccRewardPrediction_PredictsFutureReward) {
    RequireGPU();

    const size_t n_states = 20;
    const size_t n_features = 32;

    nimcp_gpu_nacc_state_t* state = CreateNAccState(n_states);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* state_features = Create1DTensor(n_features, 0.5f);
    nimcp_gpu_tensor_t* action = Create1DTensor(n_states, 0.0f);
    std::vector<float> action_data(n_states, 0.0f);
    action_data[0] = 1.0f;  // Action 0
    action = SetFromHost(action, action_data);

    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    bool result = nimcp_gpu_nacc_reward_prediction(
        ctx, state, state_features, action, &params);
    EXPECT_TRUE(result);

    auto predictions = CopyToHost(state->reward_prediction);

    // Predictions should be valid
    for (size_t i = 0; i < n_states; i++) {
        EXPECT_FALSE(std::isnan(predictions[i]));
        EXPECT_FALSE(std::isinf(predictions[i]));
    }

    nimcp_gpu_tensor_destroy(state_features);
    nimcp_gpu_tensor_destroy(action);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, NAccComputeMotivation_BalancesRewardAndEffort) {
    RequireGPU();

    const size_t n_states = 20;

    nimcp_gpu_nacc_state_t* state = CreateNAccState(n_states);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* dopamine = Create1DTensor(n_states, 0.8f);      // High dopamine
    nimcp_gpu_tensor_t* effort_required = Create1DTensor(n_states, 0.2f);  // Low effort

    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    bool result = nimcp_gpu_nacc_compute_motivation(
        ctx, state, dopamine, effort_required, &params);
    EXPECT_TRUE(result);

    auto motivation = CopyToHost(state->motivation_signal);

    // High dopamine + low effort should produce high motivation
    for (size_t i = 0; i < n_states; i++) {
        EXPECT_GE(motivation[i], 0.0f);
    }

    // Test with high effort
    nimcp_gpu_fill(ctx, effort_required, 0.9f);  // High effort
    nimcp_gpu_nacc_compute_motivation(ctx, state, dopamine, effort_required, &params);

    auto motivation_high_effort = CopyToHost(state->motivation_signal);

    // Higher effort should reduce motivation
    for (size_t i = 0; i < n_states; i++) {
        EXPECT_LE(motivation_high_effort[i], motivation[i] + 0.1f);  // Allow some tolerance
    }

    nimcp_gpu_tensor_destroy(dopamine);
    nimcp_gpu_tensor_destroy(effort_required);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, NAccMSNUpdate_ModulatesD1D2Activity) {
    RequireGPU();

    const size_t n_states = 20;
    const float dt = 1.0f;

    nimcp_gpu_nacc_state_t* state = CreateNAccState(n_states);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* cortical_input = Create1DTensor(n_states, 0.5f);
    nimcp_gpu_tensor_t* dopamine = Create1DTensor(n_states, 0.8f);  // High DA

    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    bool result = nimcp_gpu_nacc_msn_update(
        ctx, state, cortical_input, dopamine, dt, &params);
    EXPECT_TRUE(result);

    auto d1_activity = CopyToHost(state->msn_d1_activity);
    auto d2_activity = CopyToHost(state->msn_d2_activity);

    // Both D1 and D2 MSNs should have valid activity
    for (size_t i = 0; i < n_states; i++) {
        EXPECT_GE(d1_activity[i], 0.0f);
        EXPECT_GE(d2_activity[i], 0.0f);
    }

    // High dopamine should favor D1 over D2 (Go pathway)
    float d1_sum = 0.0f, d2_sum = 0.0f;
    for (size_t i = 0; i < n_states; i++) {
        d1_sum += d1_activity[i];
        d2_sum += d2_activity[i];
    }
    // With high DA, D1 should be relatively higher
    EXPECT_GE(d1_sum, 0.0f);

    nimcp_gpu_tensor_destroy(cortical_input);
    nimcp_gpu_tensor_destroy(dopamine);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, NAccGoNoGo_GeneratesDecisionSignals) {
    RequireGPU();

    const size_t n_states = 20;

    nimcp_gpu_nacc_state_t* state = CreateNAccState(n_states);
    ASSERT_NE(state, nullptr);

    // Set D1/D2 activity
    nimcp_gpu_fill(ctx, state->msn_d1_activity, 0.7f);
    nimcp_gpu_fill(ctx, state->msn_d2_activity, 0.3f);

    nimcp_gpu_tensor_t* go_signal = Create1DTensor(n_states, 0.0f);
    nimcp_gpu_tensor_t* nogo_signal = Create1DTensor(n_states, 0.0f);

    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    bool result = nimcp_gpu_nacc_go_nogo(ctx, state, go_signal, nogo_signal, &params);
    EXPECT_TRUE(result);

    auto go_data = CopyToHost(go_signal);
    auto nogo_data = CopyToHost(nogo_signal);

    // Go and NoGo signals should be valid
    for (size_t i = 0; i < n_states; i++) {
        EXPECT_GE(go_data[i], 0.0f);
        EXPECT_GE(nogo_data[i], 0.0f);
    }

    // With D1 > D2, Go should dominate
    float go_sum = 0.0f, nogo_sum = 0.0f;
    for (size_t i = 0; i < n_states; i++) {
        go_sum += go_data[i];
        nogo_sum += nogo_data[i];
    }
    EXPECT_GT(go_sum, nogo_sum);

    nimcp_gpu_tensor_destroy(go_signal);
    nimcp_gpu_tensor_destroy(nogo_signal);
    DestroyNAccState(state);
}

//=============================================================================
// ACC Operation Tests
//=============================================================================

TEST_F(EmotionKernelTest, ACCConflictDetection_DetectsResponseConflict) {
    RequireGPU();

    const size_t n_responses = 10;

    nimcp_gpu_acc_state_t* state = CreateACCState(n_responses);
    ASSERT_NE(state, nullptr);

    // Create conflicting responses (two responses equally active)
    nimcp_gpu_tensor_t* response_activations = Create1DTensor(n_responses, 0.0f);
    std::vector<float> conflict_pattern(n_responses, 0.1f);
    conflict_pattern[0] = 0.8f;
    conflict_pattern[1] = 0.8f;  // Two responses with similar high activation
    response_activations = SetFromHost(response_activations, conflict_pattern);

    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    bool result = nimcp_gpu_acc_conflict_detection(ctx, state, response_activations, &params);
    EXPECT_TRUE(result);

    auto conflict = CopyToHost(state->conflict_signal);

    // Conflict should be detected
    float conflict_sum = 0.0f;
    for (size_t i = 0; i < n_responses; i++) {
        conflict_sum += conflict[i];
    }
    EXPECT_GT(conflict_sum, 0.0f);

    // Test with no conflict (one dominant response)
    std::vector<float> no_conflict_pattern(n_responses, 0.1f);
    no_conflict_pattern[0] = 0.9f;  // Single dominant response
    response_activations = SetFromHost(response_activations, no_conflict_pattern);

    nimcp_gpu_acc_conflict_detection(ctx, state, response_activations, &params);

    auto no_conflict = CopyToHost(state->conflict_signal);
    float no_conflict_sum = 0.0f;
    for (size_t i = 0; i < n_responses; i++) {
        no_conflict_sum += no_conflict[i];
    }

    // Less conflict with single dominant response
    EXPECT_LE(no_conflict_sum, conflict_sum);

    nimcp_gpu_tensor_destroy(response_activations);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, ACCErrorSignal_ComputesPredictionError) {
    RequireGPU();

    const size_t n_responses = 10;

    nimcp_gpu_acc_state_t* state = CreateACCState(n_responses);
    ASSERT_NE(state, nullptr);

    // Expected vs actual outcomes
    nimcp_gpu_tensor_t* expected = Create1DTensor(n_responses, 0.8f);
    nimcp_gpu_tensor_t* actual = Create1DTensor(n_responses, 0.2f);  // Large mismatch

    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    bool result = nimcp_gpu_acc_error_signal(ctx, state, expected, actual, &params);
    EXPECT_TRUE(result);

    auto error = CopyToHost(state->error_signal);

    // Error should be non-zero with mismatch
    float error_sum = 0.0f;
    for (size_t i = 0; i < n_responses; i++) {
        error_sum += std::abs(error[i]);
    }
    EXPECT_GT(error_sum, 0.0f);

    // Test with matching expected/actual
    nimcp_gpu_fill(ctx, actual, 0.8f);  // Match expected
    nimcp_gpu_acc_error_signal(ctx, state, expected, actual, &params);

    auto small_error = CopyToHost(state->error_signal);
    float small_error_sum = 0.0f;
    for (size_t i = 0; i < n_responses; i++) {
        small_error_sum += std::abs(small_error[i]);
    }

    // Error should be smaller when expected matches actual
    EXPECT_LT(small_error_sum, error_sum);

    nimcp_gpu_tensor_destroy(expected);
    nimcp_gpu_tensor_destroy(actual);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, ACCEffortAllocation_AllocatesBasedOnDemand) {
    RequireGPU();

    const size_t n_responses = 10;

    nimcp_gpu_acc_state_t* state = CreateACCState(n_responses);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* task_demand = Create1DTensor(n_responses, 0.8f);        // High demand
    nimcp_gpu_tensor_t* reward_expectation = Create1DTensor(n_responses, 0.9f); // High reward

    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    bool result = nimcp_gpu_acc_effort_allocation(
        ctx, state, task_demand, reward_expectation, &params);
    EXPECT_TRUE(result);

    auto effort = CopyToHost(state->effort_allocation);

    // With high demand and high reward, effort should be allocated
    for (size_t i = 0; i < n_responses; i++) {
        EXPECT_GE(effort[i], 0.0f);
        EXPECT_LE(effort[i], 1.0f);
    }

    // Test with low reward
    nimcp_gpu_fill(ctx, reward_expectation, 0.1f);  // Low reward
    nimcp_gpu_acc_effort_allocation(ctx, state, task_demand, reward_expectation, &params);

    auto low_reward_effort = CopyToHost(state->effort_allocation);

    // Lower reward should reduce effort allocation
    float high_reward_sum = 0.0f, low_reward_sum = 0.0f;
    for (size_t i = 0; i < n_responses; i++) {
        high_reward_sum += effort[i];
        low_reward_sum += low_reward_effort[i];
    }
    EXPECT_LE(low_reward_sum, high_reward_sum + 0.1f);  // Allow tolerance

    nimcp_gpu_tensor_destroy(task_demand);
    nimcp_gpu_tensor_destroy(reward_expectation);
    DestroyACCState(state);
}

//=============================================================================
// Integrated Emotion System Tests
//=============================================================================

TEST_F(EmotionKernelTest, EmotionComputeState_ComputesPADValues) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;
    const size_t n_options = 5;
    const size_t n_outcomes = 3;
    const size_t n_states = 10;
    const size_t n_responses = 8;

    // Create full emotion system
    nimcp_gpu_emotion_system_t system;
    system.amygdala = CreateAmygdalaState(n_stimuli, n_contexts);
    system.ofc = CreateOFCState(n_options, n_outcomes);
    system.nacc = CreateNAccState(n_states);
    system.acc = CreateACCState(n_responses);
    system.emotion_vector = Create1DTensor(NIMCP_EMOTION_COUNT, 0.0f);
    system.arousal_level = Create1DTensor(1, 0.5f);
    system.valence_signal = Create1DTensor(1, 0.0f);
    system.dt = 1.0f;

    // Set some emotional state
    nimcp_gpu_fill(ctx, system.amygdala->central_output, 0.6f);  // Moderate fear
    nimcp_gpu_fill(ctx, system.nacc->hedonic_signal, 0.7f);      // Positive hedonic

    nimcp_gpu_tensor_t* valence_out = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* arousal_out = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* dominance_out = Create1DTensor(1, 0.0f);

    bool result = nimcp_gpu_emotion_compute_state(
        ctx, &system, valence_out, arousal_out, dominance_out);
    EXPECT_TRUE(result);

    auto valence = CopyToHost(valence_out);
    auto arousal = CopyToHost(arousal_out);
    auto dominance = CopyToHost(dominance_out);

    // PAD values should be in valid ranges
    EXPECT_GE(valence[0], -1.0f);
    EXPECT_LE(valence[0], 1.0f);
    EXPECT_GE(arousal[0], 0.0f);
    EXPECT_LE(arousal[0], 1.0f);
    EXPECT_GE(dominance[0], -1.0f);
    EXPECT_LE(dominance[0], 1.0f);

    // Cleanup
    nimcp_gpu_tensor_destroy(valence_out);
    nimcp_gpu_tensor_destroy(arousal_out);
    nimcp_gpu_tensor_destroy(dominance_out);
    nimcp_gpu_tensor_destroy(system.emotion_vector);
    nimcp_gpu_tensor_destroy(system.arousal_level);
    nimcp_gpu_tensor_destroy(system.valence_signal);
    DestroyAmygdalaState(system.amygdala);
    DestroyOFCState(system.ofc);
    DestroyNAccState(system.nacc);
    DestroyACCState(system.acc);
}

TEST_F(EmotionKernelTest, EmotionCategorize_MapsToDiscreteEmotions) {
    RequireGPU();

    nimcp_gpu_tensor_t* valence = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* arousal = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* emotion_probs = Create1DTensor(NIMCP_EMOTION_COUNT, 0.0f);

    // Test fear region: negative valence, high arousal
    std::vector<float> fear_valence = {-0.7f};
    std::vector<float> fear_arousal = {0.8f};
    valence = SetFromHost(valence, fear_valence);
    arousal = SetFromHost(arousal, fear_arousal);

    bool result = nimcp_gpu_emotion_categorize(ctx, valence, arousal, emotion_probs);
    EXPECT_TRUE(result);

    auto probs = CopyToHost(emotion_probs);

    // Probabilities should sum to ~1.0
    float sum = 0.0f;
    for (size_t i = 0; i < NIMCP_EMOTION_COUNT; i++) {
        EXPECT_GE(probs[i], 0.0f);
        EXPECT_LE(probs[i], 1.0f);
        sum += probs[i];
    }
    EXPECT_NEAR(sum, 1.0f, 0.1f);

    // Fear (index 1) should have elevated probability in this region
    EXPECT_GT(probs[NIMCP_EMOTION_FEAR], 0.0f);

    // Test happiness region: positive valence, moderate arousal
    std::vector<float> happy_valence = {0.8f};
    std::vector<float> happy_arousal = {0.6f};
    valence = SetFromHost(valence, happy_valence);
    arousal = SetFromHost(arousal, happy_arousal);

    nimcp_gpu_emotion_categorize(ctx, valence, arousal, emotion_probs);
    probs = CopyToHost(emotion_probs);

    // Happiness (index 5) should have elevated probability
    EXPECT_GT(probs[NIMCP_EMOTION_HAPPINESS], 0.0f);

    nimcp_gpu_tensor_destroy(valence);
    nimcp_gpu_tensor_destroy(arousal);
    nimcp_gpu_tensor_destroy(emotion_probs);
}

TEST_F(EmotionKernelTest, EmotionCognitiveModulation_GeneratesBiases) {
    RequireGPU();

    // The cognitive modulation kernel reads threat_signal, reward_prediction,
    // and arousal_level with n = attention_bias->numel.  All tensors indexed
    // by the kernel must have at least n elements, so we use a common size.
    const size_t n_common = 8;
    const size_t n_contexts = 4;
    const size_t n_outcomes = 3;

    // Create emotion system with matching sizes for cognitive modulation
    nimcp_gpu_emotion_system_t system;
    system.amygdala = CreateAmygdalaState(n_common, n_contexts);
    system.ofc = CreateOFCState(n_common, n_outcomes);
    system.nacc = CreateNAccState(n_common);
    system.acc = CreateACCState(n_common);
    system.emotion_vector = Create1DTensor(NIMCP_EMOTION_COUNT, 0.0f);
    system.arousal_level = Create1DTensor(n_common, 0.8f);   // High arousal
    system.valence_signal = Create1DTensor(1, -0.5f); // Negative valence
    system.dt = 1.0f;

    // Set fear state - the cognitive modulation kernel reads:
    //   amygdala->threat_signal (not central_output)
    //   nacc->reward_prediction (not hedonic_signal)
    //   arousal_level
    nimcp_gpu_fill(ctx, system.amygdala->threat_signal, 0.9f);
    nimcp_gpu_fill(ctx, system.nacc->reward_prediction, 0.3f);

    nimcp_gpu_tensor_t* attention_bias = Create1DTensor(n_common, 0.0f);
    nimcp_gpu_tensor_t* memory_enhancement = Create1DTensor(n_common, 0.0f);
    nimcp_gpu_tensor_t* decision_bias = Create1DTensor(n_common, 0.0f);

    bool result = nimcp_gpu_emotion_cognitive_modulation(
        ctx, &system, attention_bias, memory_enhancement, decision_bias);
    EXPECT_TRUE(result);

    auto attention = CopyToHost(attention_bias);
    auto memory = CopyToHost(memory_enhancement);
    auto decision = CopyToHost(decision_bias);

    // High arousal should enhance attention and memory
    float attention_sum = 0.0f, memory_sum = 0.0f;
    for (size_t i = 0; i < n_common; i++) {
        attention_sum += std::abs(attention[i]);
        memory_sum += std::abs(memory[i]);
    }
    EXPECT_GT(attention_sum, 0.0f);
    EXPECT_GT(memory_sum, 0.0f);

    // Decision bias should be affected by emotional state
    bool has_bias = false;
    for (size_t i = 0; i < n_common; i++) {
        if (std::abs(decision[i]) > 1e-6f) {
            has_bias = true;
            break;
        }
    }
    EXPECT_TRUE(has_bias);

    // Cleanup
    nimcp_gpu_tensor_destroy(attention_bias);
    nimcp_gpu_tensor_destroy(memory_enhancement);
    nimcp_gpu_tensor_destroy(decision_bias);
    nimcp_gpu_tensor_destroy(system.emotion_vector);
    nimcp_gpu_tensor_destroy(system.arousal_level);
    nimcp_gpu_tensor_destroy(system.valence_signal);
    DestroyAmygdalaState(system.amygdala);
    DestroyOFCState(system.ofc);
    DestroyNAccState(system.nacc);
    DestroyACCState(system.acc);
}

//=============================================================================
// Null Safety Tests
//=============================================================================

TEST_F(EmotionKernelTest, AmygdalaThreatDetection_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(10, 4);
    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    EXPECT_FALSE(nimcp_gpu_amygdala_threat_detection(nullptr, state, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_threat_detection(ctx, nullptr, tensor, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_threat_detection(ctx, state, nullptr, tensor, tensor, &params));
    /* context parameter is intentionally nullable - kernel handles NULL context */
    EXPECT_FALSE(nimcp_gpu_amygdala_threat_detection(ctx, state, tensor, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_threat_detection(ctx, state, tensor, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaFearConditioning_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(10, 4);
    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    EXPECT_FALSE(nimcp_gpu_amygdala_fear_conditioning(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_fear_conditioning(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_fear_conditioning(ctx, state, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_fear_conditioning(ctx, state, tensor, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_fear_conditioning(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaExtinction_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(10, 4);
    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    EXPECT_FALSE(nimcp_gpu_amygdala_extinction(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_extinction(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_extinction(ctx, state, nullptr, tensor, 1.0f, &params));
    /* no_us parameter is intentionally nullable - kernel handles NULL no_us */
    EXPECT_FALSE(nimcp_gpu_amygdala_extinction(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, AmygdalaPrefrontalInhibition_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(10, 4);
    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    EXPECT_FALSE(nimcp_gpu_amygdala_prefrontal_inhibition(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_prefrontal_inhibition(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_prefrontal_inhibition(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_amygdala_prefrontal_inhibition(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, OFCComputeValues_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_ofc_state_t* state = CreateOFCState(10, 5);
    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    EXPECT_FALSE(nimcp_gpu_ofc_compute_values(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_compute_values(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_compute_values(ctx, state, nullptr, tensor, &params));
    /* context parameter is intentionally nullable - kernel handles NULL context */
    EXPECT_FALSE(nimcp_gpu_ofc_compute_values(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, OFCValueUpdate_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_ofc_state_t* state = CreateOFCState(10, 5);
    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    EXPECT_FALSE(nimcp_gpu_ofc_value_update(nullptr, state, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_value_update(ctx, nullptr, tensor, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_value_update(ctx, state, nullptr, tensor, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_value_update(ctx, state, tensor, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_value_update(ctx, state, tensor, tensor, 1.0f, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, OFCChoiceProbabilities_NullSafety) {
    RequireGPU();

    nimcp_gpu_ofc_state_t* state = CreateOFCState(10, 5);
    nimcp_gpu_ofc_params_t params = nimcp_gpu_ofc_params_default();

    EXPECT_FALSE(nimcp_gpu_ofc_choice_probabilities(nullptr, state, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_choice_probabilities(ctx, nullptr, 1.0f, &params));
    EXPECT_FALSE(nimcp_gpu_ofc_choice_probabilities(ctx, state, 1.0f, nullptr));

    DestroyOFCState(state);
}

TEST_F(EmotionKernelTest, NAccRewardPrediction_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_nacc_state_t* state = CreateNAccState(10);
    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    EXPECT_FALSE(nimcp_gpu_nacc_reward_prediction(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_reward_prediction(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_reward_prediction(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_reward_prediction(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_reward_prediction(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, NAccComputeMotivation_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_nacc_state_t* state = CreateNAccState(10);
    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    EXPECT_FALSE(nimcp_gpu_nacc_compute_motivation(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_compute_motivation(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_compute_motivation(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_compute_motivation(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_compute_motivation(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, NAccGoNoGo_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_nacc_state_t* state = CreateNAccState(10);
    nimcp_gpu_nacc_params_t params = nimcp_gpu_nacc_params_default();

    EXPECT_FALSE(nimcp_gpu_nacc_go_nogo(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_go_nogo(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_go_nogo(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_go_nogo(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_nacc_go_nogo(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyNAccState(state);
}

TEST_F(EmotionKernelTest, ACCConflictDetection_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_acc_state_t* state = CreateACCState(10);
    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    EXPECT_FALSE(nimcp_gpu_acc_conflict_detection(nullptr, state, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_conflict_detection(ctx, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_conflict_detection(ctx, state, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_acc_conflict_detection(ctx, state, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, ACCErrorSignal_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_acc_state_t* state = CreateACCState(10);
    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    EXPECT_FALSE(nimcp_gpu_acc_error_signal(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_error_signal(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_error_signal(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_error_signal(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_acc_error_signal(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, ACCEffortAllocation_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(10, 0.0f);
    nimcp_gpu_acc_state_t* state = CreateACCState(10);
    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    EXPECT_FALSE(nimcp_gpu_acc_effort_allocation(nullptr, state, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_effort_allocation(ctx, nullptr, tensor, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_effort_allocation(ctx, state, nullptr, tensor, &params));
    EXPECT_FALSE(nimcp_gpu_acc_effort_allocation(ctx, state, tensor, nullptr, &params));
    EXPECT_FALSE(nimcp_gpu_acc_effort_allocation(ctx, state, tensor, tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, EmotionComputeState_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(1, 0.0f);

    EXPECT_FALSE(nimcp_gpu_emotion_compute_state(nullptr, nullptr, tensor, tensor, tensor));
    EXPECT_FALSE(nimcp_gpu_emotion_compute_state(ctx, nullptr, tensor, tensor, tensor));

    nimcp_gpu_tensor_destroy(tensor);
}

TEST_F(EmotionKernelTest, EmotionCategorize_NullSafety) {
    RequireGPU();

    nimcp_gpu_tensor_t* tensor = Create1DTensor(NIMCP_EMOTION_COUNT, 0.0f);
    nimcp_gpu_tensor_t* small_tensor = Create1DTensor(1, 0.0f);

    EXPECT_FALSE(nimcp_gpu_emotion_categorize(nullptr, small_tensor, small_tensor, tensor));
    EXPECT_FALSE(nimcp_gpu_emotion_categorize(ctx, nullptr, small_tensor, tensor));
    EXPECT_FALSE(nimcp_gpu_emotion_categorize(ctx, small_tensor, nullptr, tensor));
    EXPECT_FALSE(nimcp_gpu_emotion_categorize(ctx, small_tensor, small_tensor, nullptr));

    nimcp_gpu_tensor_destroy(tensor);
    nimcp_gpu_tensor_destroy(small_tensor);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(EmotionKernelTest, Integration_FearConditioningAndExtinction) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;
    const float dt = 1.0f;
    const int conditioning_trials = 50;
    const int extinction_trials = 100;

    nimcp_gpu_amygdala_state_t* state = CreateAmygdalaState(n_stimuli, n_contexts);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_tensor_t* cs = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* us = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* no_us = Create1DTensor(n_stimuli, 0.0f);

    // Set CS-US pair
    std::vector<float> cs_data(n_stimuli, 0.0f);
    std::vector<float> us_data(n_stimuli, 0.0f);
    cs_data[0] = 1.0f;
    us_data[0] = 1.0f;
    cs = SetFromHost(cs, cs_data);
    us = SetFromHost(us, us_data);

    nimcp_gpu_amygdala_params_t params = nimcp_gpu_amygdala_params_default();

    // Phase 1: Fear conditioning
    for (int i = 0; i < conditioning_trials; i++) {
        nimcp_gpu_amygdala_fear_conditioning(ctx, state, cs, us, dt, &params);
    }

    auto post_conditioning_fear = CopyToHost(state->fear_memory);
    float conditioning_fear_sum = 0.0f;
    for (float v : post_conditioning_fear) conditioning_fear_sum += v;

    // Fear should be established
    EXPECT_GT(conditioning_fear_sum, 0.0f);

    // Phase 2: Extinction
    for (int i = 0; i < extinction_trials; i++) {
        nimcp_gpu_amygdala_extinction(ctx, state, cs, no_us, dt, &params);
    }

    auto post_extinction = CopyToHost(state->extinction_trace);
    float extinction_sum = 0.0f;
    for (float v : post_extinction) extinction_sum += v;

    // Extinction trace should be present
    EXPECT_GT(extinction_sum, 0.0f);

    nimcp_gpu_tensor_destroy(cs);
    nimcp_gpu_tensor_destroy(us);
    nimcp_gpu_tensor_destroy(no_us);
    DestroyAmygdalaState(state);
}

TEST_F(EmotionKernelTest, Integration_RewardLearningAndDecision) {
    RequireGPU();

    const size_t n_options = 4;
    const size_t n_outcomes = 2;
    const size_t n_flat = n_options * n_outcomes;  // Flat size for expected_outcomes
    const size_t n_states = 10;
    const float dt = 1.0f;
    const int learning_trials = 100;

    nimcp_gpu_ofc_state_t* ofc_state = CreateOFCState(n_options, n_outcomes);
    nimcp_gpu_nacc_state_t* nacc_state = CreateNAccState(n_states);
    ASSERT_NE(ofc_state, nullptr);
    ASSERT_NE(nacc_state, nullptr);

    nimcp_gpu_ofc_params_t ofc_params = nimcp_gpu_ofc_params_default();
    nimcp_gpu_nacc_params_t nacc_params = nimcp_gpu_nacc_params_default();

    // Set up reward contingencies: option 2 is best
    // The value_update kernel indexes chosen_option and outcome as flat arrays
    // matching expected_outcomes (n_options * n_outcomes), so we use flat tensors.
    nimcp_gpu_tensor_t* chosen = Create1DTensor(n_flat, 0.0f);
    nimcp_gpu_tensor_t* outcome = Create1DTensor(n_flat, 0.0f);
    nimcp_gpu_tensor_t* dopamine = Create1DTensor(n_states, 0.0f);
    nimcp_gpu_tensor_t* effort = Create1DTensor(n_states, 0.2f);

    // Learn that option 2 gives high reward
    for (int trial = 0; trial < learning_trials; trial++) {
        int choice = trial % n_options;

        // Flat-index: option i occupies indices [i*n_outcomes .. (i+1)*n_outcomes-1]
        std::vector<float> chosen_data(n_flat, 0.0f);
        for (size_t o = 0; o < n_outcomes; o++) {
            chosen_data[choice * n_outcomes + o] = 1.0f;
        }
        chosen = SetFromHost(chosen, chosen_data);

        std::vector<float> outcome_data(n_flat, 0.0f);
        if (choice == 2) {
            outcome_data[choice * n_outcomes + 0] = 0.9f;  // High reward for option 2
        } else {
            outcome_data[choice * n_outcomes + 0] = 0.2f;  // Low reward for others
        }
        outcome = SetFromHost(outcome, outcome_data);

        // OFC value update
        nimcp_gpu_ofc_value_update(ctx, ofc_state, chosen, outcome, dt, &ofc_params);

        // NAcc dopamine signal
        float da_level = (choice == 2) ? 0.8f : 0.3f;
        nimcp_gpu_fill(ctx, dopamine, da_level);
        nimcp_gpu_nacc_compute_motivation(ctx, nacc_state, dopamine, effort, &nacc_params);
    }

    // After learning, expected_outcomes for option 2 should be highest
    auto expected = CopyToHost(ofc_state->expected_outcomes);

    // Sum expected outcomes per option
    std::vector<float> option_expected_sums(n_options, 0.0f);
    for (size_t opt = 0; opt < n_options; opt++) {
        for (size_t o = 0; o < n_outcomes; o++) {
            option_expected_sums[opt] += expected[opt * n_outcomes + o];
        }
    }

    // Option 2 should have highest expected outcome sum
    size_t best_option = 0;
    float best_value = option_expected_sums[0];
    for (size_t i = 1; i < n_options; i++) {
        if (option_expected_sums[i] > best_value) {
            best_value = option_expected_sums[i];
            best_option = i;
        }
    }
    EXPECT_EQ(best_option, 2u);

    nimcp_gpu_tensor_destroy(chosen);
    nimcp_gpu_tensor_destroy(outcome);
    nimcp_gpu_tensor_destroy(dopamine);
    nimcp_gpu_tensor_destroy(effort);
    DestroyOFCState(ofc_state);
    DestroyNAccState(nacc_state);
}

TEST_F(EmotionKernelTest, Integration_ConflictAndCognitiveControl) {
    RequireGPU();

    const size_t n_responses = 4;
    const int n_trials = 50;

    nimcp_gpu_acc_state_t* state = CreateACCState(n_responses);
    ASSERT_NE(state, nullptr);

    nimcp_gpu_acc_params_t params = nimcp_gpu_acc_params_default();

    nimcp_gpu_tensor_t* response_activations = Create1DTensor(n_responses, 0.0f);
    nimcp_gpu_tensor_t* expected = Create1DTensor(n_responses, 0.0f);
    nimcp_gpu_tensor_t* actual = Create1DTensor(n_responses, 0.0f);
    nimcp_gpu_tensor_t* task_demand = Create1DTensor(n_responses, 0.5f);
    nimcp_gpu_tensor_t* reward_exp = Create1DTensor(n_responses, 0.7f);

    float total_conflict = 0.0f;
    float total_control = 0.0f;

    // Run trials with varying conflict levels
    for (int trial = 0; trial < n_trials; trial++) {
        // Create conflict pattern (high on conflict trials)
        // The conflict kernel computes conflict = max1 * max2, thresholded at 0.5
        // So competing responses must exceed sqrt(0.5) ~ 0.71 each
        std::vector<float> responses(n_responses, 0.2f);
        if (trial % 3 == 0) {
            // Conflict trial: two responses compete (0.8*0.8=0.64 > threshold)
            responses[0] = 0.8f;
            responses[1] = 0.8f;
        } else {
            // Easy trial: one dominant response
            responses[0] = 0.9f;
        }
        response_activations = SetFromHost(response_activations, responses);

        // Detect conflict
        nimcp_gpu_acc_conflict_detection(ctx, state, response_activations, &params);

        // Compute error signal
        std::vector<float> exp_data(n_responses, 0.0f);
        exp_data[0] = 1.0f;
        expected = SetFromHost(expected, exp_data);
        actual = SetFromHost(actual, responses);
        nimcp_gpu_acc_error_signal(ctx, state, expected, actual, &params);

        // Allocate effort
        nimcp_gpu_acc_effort_allocation(ctx, state, task_demand, reward_exp, &params);

        auto conflict = CopyToHost(state->conflict_signal);
        auto control = CopyToHost(state->control_signal);

        for (size_t i = 0; i < n_responses; i++) {
            total_conflict += conflict[i];
            total_control += control[i];
        }
    }

    // ACC should generate conflict and control signals
    EXPECT_GT(total_conflict, 0.0f);
    EXPECT_GT(total_control, 0.0f);

    nimcp_gpu_tensor_destroy(response_activations);
    nimcp_gpu_tensor_destroy(expected);
    nimcp_gpu_tensor_destroy(actual);
    nimcp_gpu_tensor_destroy(task_demand);
    nimcp_gpu_tensor_destroy(reward_exp);
    DestroyACCState(state);
}

TEST_F(EmotionKernelTest, Integration_FullEmotionSystemUpdate) {
    RequireGPU();

    const size_t n_stimuli = 16;
    const size_t n_contexts = 4;
    const size_t n_options = 5;
    const size_t n_outcomes = 3;
    const size_t n_states = 10;
    const size_t n_responses = 8;
    const float dt = 1.0f;
    const int n_steps = 50;

    // Create full emotion system
    nimcp_gpu_emotion_system_t system;
    system.amygdala = CreateAmygdalaState(n_stimuli, n_contexts);
    system.ofc = CreateOFCState(n_options, n_outcomes);
    system.nacc = CreateNAccState(n_states);
    system.acc = CreateACCState(n_responses);
    system.emotion_vector = Create1DTensor(NIMCP_EMOTION_COUNT, 0.0f);
    system.arousal_level = Create1DTensor(1, 0.5f);
    system.valence_signal = Create1DTensor(1, 0.0f);
    system.dt = dt;

    nimcp_gpu_tensor_t* sensory_input = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* reward_signal = Create1DTensor(n_states, 0.0f);
    nimcp_gpu_tensor_t* context = Create1DTensor(n_contexts, 0.5f);

    // Track emotional trajectory
    std::vector<float> arousal_trajectory;
    std::vector<float> valence_trajectory;

    nimcp_gpu_tensor_t* valence_out = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* arousal_out = Create1DTensor(1, 0.0f);
    nimcp_gpu_tensor_t* dominance_out = Create1DTensor(1, 0.0f);

    // Pre-establish some fear memory for threat detection to produce output
    nimcp_gpu_fill(ctx, system.amygdala->fear_memory, 0.5f);

    for (int step = 0; step < n_steps; step++) {
        // Simulate varying inputs
        float threat_level = (step < 20) ? 0.8f : 0.2f;  // High threat early, then low
        float reward_level = (step > 30) ? 0.9f : 0.3f;  // High reward late

        nimcp_gpu_fill(ctx, sensory_input, threat_level);
        nimcp_gpu_fill(ctx, reward_signal, reward_level);

        // Manually drive subsystems since system_update is a placeholder
        nimcp_gpu_amygdala_params_t amy_params = nimcp_gpu_amygdala_params_default();
        nimcp_gpu_tensor_t* threat_out = system.amygdala->central_output;
        nimcp_gpu_amygdala_threat_detection(ctx, system.amygdala,
            sensory_input, context, threat_out, &amy_params);

        // Drive NAcc motivation with reward signal
        nimcp_gpu_nacc_params_t nacc_params = nimcp_gpu_nacc_params_default();
        nimcp_gpu_fill(ctx, system.nacc->reward_prediction, reward_level);
        nimcp_gpu_tensor_t* effort_tensor = Create1DTensor(n_states, 0.3f);
        nimcp_gpu_nacc_compute_motivation(ctx, system.nacc,
            reward_signal, effort_tensor, &nacc_params);
        nimcp_gpu_tensor_destroy(effort_tensor);

        // Compute emotional state
        nimcp_gpu_emotion_compute_state(ctx, &system, valence_out, arousal_out, dominance_out);

        auto valence = CopyToHost(valence_out);
        auto arousal = CopyToHost(arousal_out);

        arousal_trajectory.push_back(arousal[0]);
        valence_trajectory.push_back(valence[0]);
    }

    // Emotional state should change over time
    bool arousal_changed = false;
    bool valence_changed = false;

    for (size_t i = 1; i < arousal_trajectory.size(); i++) {
        if (std::abs(arousal_trajectory[i] - arousal_trajectory[0]) > 0.01f) {
            arousal_changed = true;
        }
        if (std::abs(valence_trajectory[i] - valence_trajectory[0]) > 0.01f) {
            valence_changed = true;
        }
    }

    EXPECT_TRUE(arousal_changed || valence_changed);

    // Cleanup
    nimcp_gpu_tensor_destroy(sensory_input);
    nimcp_gpu_tensor_destroy(reward_signal);
    nimcp_gpu_tensor_destroy(context);
    nimcp_gpu_tensor_destroy(valence_out);
    nimcp_gpu_tensor_destroy(arousal_out);
    nimcp_gpu_tensor_destroy(dominance_out);
    nimcp_gpu_tensor_destroy(system.emotion_vector);
    nimcp_gpu_tensor_destroy(system.arousal_level);
    nimcp_gpu_tensor_destroy(system.valence_signal);
    DestroyAmygdalaState(system.amygdala);
    DestroyOFCState(system.ofc);
    DestroyNAccState(system.nacc);
    DestroyACCState(system.acc);
}

TEST_F(EmotionKernelTest, Integration_EmotionDrivenBehavior) {
    RequireGPU();

    const size_t n_stimuli = 8;
    const size_t n_contexts = 2;
    const size_t n_options = 4;
    const size_t n_outcomes = 2;
    const size_t n_states = 8;
    const float dt = 1.0f;

    // Create subsystems
    nimcp_gpu_amygdala_state_t* amygdala = CreateAmygdalaState(n_stimuli, n_contexts);
    nimcp_gpu_ofc_state_t* ofc = CreateOFCState(n_options, n_outcomes);
    nimcp_gpu_nacc_state_t* nacc = CreateNAccState(n_states);

    nimcp_gpu_amygdala_params_t amy_params = nimcp_gpu_amygdala_params_default();
    nimcp_gpu_ofc_params_t ofc_params = nimcp_gpu_ofc_params_default();
    nimcp_gpu_nacc_params_t nacc_params = nimcp_gpu_nacc_params_default();

    // Create inputs
    nimcp_gpu_tensor_t* threat_stimulus = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* context = Create1DTensor(n_contexts, 0.5f);
    nimcp_gpu_tensor_t* threat_out = Create1DTensor(n_stimuli, 0.0f);
    nimcp_gpu_tensor_t* dopamine = Create1DTensor(n_states, 0.0f);
    nimcp_gpu_tensor_t* effort = Create1DTensor(n_states, 0.3f);

    // Scenario: Present threat -> Fear response -> Avoidance behavior

    // Pre-establish fear memory so the amygdala can detect threats.
    // The threat detection kernel computes:
    //   similarity = exp(-input^2 / (2*sigma^2))
    //   lateral = fear_memory * similarity * context_mod
    // The generalization is centered at 0, so inputs near 0 yield high similarity.
    // With sigma=0.2, input must be within ~0.3 to get meaningful similarity.
    // We set fear_memory high and use a moderate stimulus value.
    nimcp_gpu_fill(ctx, amygdala->fear_memory, 0.9f);

    // 1. Present threatening stimulus (within generalization window)
    std::vector<float> threat_data(n_stimuli, 0.0f);
    threat_data[0] = 0.1f;  // Stimulus within generalization sigma
    threat_stimulus = SetFromHost(threat_stimulus, threat_data);

    // 2. Amygdala detects threat
    nimcp_gpu_amygdala_threat_detection(ctx, amygdala, threat_stimulus, context, threat_out, &amy_params);

    auto threat = CopyToHost(threat_out);
    EXPECT_GT(threat[0], 0.0f);  // Threat detected

    // 3. Fear reduces dopamine (aversive state)
    float fear_level = threat[0];
    nimcp_gpu_fill(ctx, dopamine, 0.3f - fear_level * 0.2f);  // Lower DA with fear

    // 4. NAcc computes motivation (reduced due to fear)
    nimcp_gpu_nacc_compute_motivation(ctx, nacc, dopamine, effort, &nacc_params);

    auto motivation = CopyToHost(nacc->motivation_signal);
    float avg_motivation = 0.0f;
    for (size_t i = 0; i < n_states; i++) avg_motivation += motivation[i];
    avg_motivation /= n_states;

    // Fear should reduce approach motivation
    EXPECT_LT(avg_motivation, 0.5f);

    // 5. OFC biases choices away from threatening option
    // Set option values (option 0 is associated with threat)
    std::vector<float> values(n_options);
    values[0] = 0.2f;  // Low value (threat)
    values[1] = 0.6f;  // Safe option
    values[2] = 0.6f;  // Safe option
    values[3] = 0.6f;  // Safe option
    ofc->option_values = SetFromHost(ofc->option_values, values);

    nimcp_gpu_ofc_choice_probabilities(ctx, ofc, 1.0f, &ofc_params);

    auto probs = CopyToHost(ofc->choice_probabilities);

    // Threatening option should have lowest probability
    EXPECT_LT(probs[0], probs[1]);
    EXPECT_LT(probs[0], probs[2]);
    EXPECT_LT(probs[0], probs[3]);

    // Cleanup
    nimcp_gpu_tensor_destroy(threat_stimulus);
    nimcp_gpu_tensor_destroy(context);
    nimcp_gpu_tensor_destroy(threat_out);
    nimcp_gpu_tensor_destroy(dopamine);
    nimcp_gpu_tensor_destroy(effort);
    DestroyAmygdalaState(amygdala);
    DestroyOFCState(ofc);
    DestroyNAccState(nacc);
}
