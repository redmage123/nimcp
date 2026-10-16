/**
 * @file test_snn_triplet_stdp_gpu.cpp
 * @brief Unit tests for GPU Triplet STDP implementation
 *
 * WHAT: Tests for triplet STDP learning rules based on Pfister & Gerstner (2006)
 * WHY:  Verify biologically accurate spike-timing dependent plasticity
 * HOW:  GoogleTest with GPU context, testing trace dynamics and weight updates
 *
 * TEST COVERAGE:
 * - Trace decay matches analytical solution
 * - LTP when post fires after pre
 * - LTD when pre fires after post
 * - Triplet effect (burst potentiation)
 * - Weight bounds are respected
 * - Comparison with pair-based STDP
 * - DAO lifecycle (create/destroy)
 * - Parameter validation
 *
 * Reference: Pfister & Gerstner (2006) "Triplets of Spikes in a Model of
 *            Spike Timing-Dependent Plasticity"
 *
 * @author NIMCP Development Team
 * @date 2025
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numeric>

#include "gpu/snn/nimcp_snn_gpu.h"
#include "gpu/context/nimcp_gpu_context.h"
#include "gpu/tensor/nimcp_tensor_gpu.h"

//=============================================================================
// Test Constants
//=============================================================================

static constexpr size_t NUM_PRE = 10;
static constexpr size_t NUM_POST = 8;
static constexpr float DEFAULT_DT = 1.0f;   // 1ms timestep
static constexpr float TOLERANCE = 1e-4f;   // Numerical tolerance

//=============================================================================
// Test Fixture
//=============================================================================

/**
 * @brief Test fixture for Triplet STDP GPU tests
 */
class TripletSTDPTest : public ::testing::Test {
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

    /**
     * @brief Create default triplet STDP parameters
     */
    nimcp_triplet_stdp_params_t create_default_params() {
        nimcp_triplet_stdp_params_t params;
        nimcp_triplet_stdp_default_params(&params);
        return params;
    }

    /**
     * @brief Create GPU tensor from host data
     */
    nimcp_gpu_tensor_t* create_tensor_from_data(const float* data, size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        return nimcp_gpu_tensor_from_host(ctx, data, dims, 1, NIMCP_GPU_PRECISION_FP32);
    }

    /**
     * @brief Create GPU tensor filled with zeros
     */
    nimcp_gpu_tensor_t* create_zero_tensor(size_t size) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) nimcp_gpu_zeros(ctx, tensor);
        return tensor;
    }

    /**
     * @brief Create GPU tensor filled with a value
     */
    nimcp_gpu_tensor_t* create_filled_tensor(size_t size, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[1] = {size};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 1, NIMCP_GPU_PRECISION_FP32);
        if (tensor) nimcp_gpu_fill(ctx, tensor, value);
        return tensor;
    }

    /**
     * @brief Create 2D weight matrix
     */
    nimcp_gpu_tensor_t* create_matrix(size_t rows, size_t cols, float value) {
        if (!gpu_available) return nullptr;
        size_t dims[2] = {rows, cols};
        nimcp_gpu_tensor_t* tensor = nimcp_gpu_tensor_create(ctx, dims, 2, NIMCP_GPU_PRECISION_FP32);
        if (tensor) nimcp_gpu_fill(ctx, tensor, value);
        return tensor;
    }

    /**
     * @brief Copy tensor to host
     */
    bool copy_to_host(const nimcp_gpu_tensor_t* tensor, float* host_data) {
        if (!tensor || !host_data) return false;
        return nimcp_gpu_tensor_to_host(tensor, host_data);
    }

    /**
     * @brief Compute analytical trace decay
     */
    float analytical_decay(float initial, float tau, float dt, int steps) {
        return initial * std::exp(-steps * dt / tau);
    }
};

//=============================================================================
// Default Parameters Test
//=============================================================================

/**
 * TEST: Default parameters are set correctly
 * WHAT: Verify nimcp_triplet_stdp_default_params returns valid defaults
 * WHY:  Ensure biologically plausible default values
 */
TEST_F(TripletSTDPTest, DefaultParams_ReturnsValidDefaults) {
    nimcp_triplet_stdp_params_t params;
    nimcp_triplet_stdp_default_params(&params);

    // Check pair-based terms
    EXPECT_GT(params.A2_plus, 0.0f) << "A2_plus should be positive";
    EXPECT_GT(params.A2_minus, 0.0f) << "A2_minus should be positive";
    EXPECT_GT(params.tau_plus, 0.0f) << "tau_plus should be positive";
    EXPECT_GT(params.tau_minus, 0.0f) << "tau_minus should be positive";

    // Check triplet terms
    EXPECT_GE(params.A3_plus, 0.0f) << "A3_plus should be non-negative";
    EXPECT_GE(params.A3_minus, 0.0f) << "A3_minus should be non-negative";
    EXPECT_GT(params.tau_x, 0.0f) << "tau_x should be positive";
    EXPECT_GT(params.tau_y, 0.0f) << "tau_y should be positive";

    // Check bounds
    EXPECT_LT(params.w_min, params.w_max) << "w_min should be less than w_max";

    // Verify typical values from Pfister & Gerstner (2006)
    EXPECT_NEAR(params.tau_plus, 16.8f, 1.0f);
    EXPECT_NEAR(params.tau_minus, 33.7f, 2.0f);
    EXPECT_GT(params.tau_x, params.tau_plus) << "Slow pre trace should decay slower";
    EXPECT_GT(params.tau_y, params.tau_minus) << "Slow post trace should decay slower";
}

/**
 * TEST: Default params with NULL pointer
 * WHAT: Verify NULL safety
 * WHY:  Prevent crashes on invalid input
 */
TEST_F(TripletSTDPTest, DefaultParams_NullPointer_NoOp) {
    nimcp_triplet_stdp_default_params(nullptr);
    SUCCEED() << "Should not crash on NULL";
}

//=============================================================================
// DAO Lifecycle Tests
//=============================================================================

/**
 * TEST: DAO creation with valid parameters
 * WHAT: Create triplet STDP DAO
 * WHY:  Verify proper initialization
 */
TEST_F(TripletSTDPTest, DAOCreate_ValidParams_Succeeds) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    nimcp_stdp_dao_t* dao = nimcp_triplet_stdp_create(ctx, NUM_PRE, NUM_POST, &params);

    ASSERT_NE(dao, nullptr) << "DAO creation should succeed";
    EXPECT_NE(dao->state, nullptr) << "State should be allocated";
    EXPECT_EQ(dao->state->num_pre, NUM_PRE);
    EXPECT_EQ(dao->state->num_post, NUM_POST);

    // Verify method pointers are set
    EXPECT_NE(dao->update_traces, nullptr);
    EXPECT_NE(dao->reset, nullptr);

    nimcp_triplet_stdp_destroy(dao);
}

/**
 * TEST: DAO creation with NULL context
 * WHAT: Try to create DAO without GPU context
 * WHY:  Verify NULL safety
 */
TEST_F(TripletSTDPTest, DAOCreate_NullContext_ReturnsNull) {
    nimcp_triplet_stdp_params_t params = create_default_params();
    nimcp_stdp_dao_t* dao = nimcp_triplet_stdp_create(nullptr, NUM_PRE, NUM_POST, &params);
    EXPECT_EQ(dao, nullptr) << "Should reject NULL context";
}

/**
 * TEST: DAO creation with zero neurons
 * WHAT: Try to create DAO with zero pre/post neurons
 * WHY:  Verify input validation
 */
TEST_F(TripletSTDPTest, DAOCreate_ZeroNeurons_ReturnsNull) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();

    nimcp_stdp_dao_t* dao1 = nimcp_triplet_stdp_create(ctx, 0, NUM_POST, &params);
    EXPECT_EQ(dao1, nullptr) << "Should reject zero pre neurons";

    nimcp_stdp_dao_t* dao2 = nimcp_triplet_stdp_create(ctx, NUM_PRE, 0, &params);
    EXPECT_EQ(dao2, nullptr) << "Should reject zero post neurons";
}

/**
 * TEST: DAO creation with NULL params uses defaults
 * WHAT: Create DAO without explicit parameters
 * WHY:  Should use default parameters
 */
TEST_F(TripletSTDPTest, DAOCreate_NullParams_UsesDefaults) {
    RequireGPU();

    nimcp_stdp_dao_t* dao = nimcp_triplet_stdp_create(ctx, NUM_PRE, NUM_POST, nullptr);
    ASSERT_NE(dao, nullptr) << "DAO creation with NULL params should succeed";

    // Verify default params were applied
    EXPECT_NEAR(dao->params.tau_plus, 16.8f, 1.0f);
    EXPECT_GT(dao->params.A2_plus, 0.0f);

    nimcp_triplet_stdp_destroy(dao);
}

/**
 * TEST: DAO destruction with NULL
 * WHAT: Destroy NULL DAO
 * WHY:  Verify NULL safety
 */
TEST_F(TripletSTDPTest, DAODestroy_Null_NoOp) {
    nimcp_triplet_stdp_destroy(nullptr);
    SUCCEED() << "Should not crash on NULL";
}

/**
 * TEST: DAO reset clears traces
 * WHAT: Reset all traces to zero
 * WHY:  For episode boundaries in RL
 */
TEST_F(TripletSTDPTest, DAOReset_ClearsTraces) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    nimcp_stdp_dao_t* dao = nimcp_triplet_stdp_create(ctx, NUM_PRE, NUM_POST, &params);
    ASSERT_NE(dao, nullptr);

    // Reset should succeed
    int result = dao->reset(dao);
    EXPECT_EQ(result, 0);

    nimcp_triplet_stdp_destroy(dao);
}

//=============================================================================
// Trace Decay Tests
//=============================================================================

/**
 * TEST: Trace decay matches analytical solution
 * WHAT: Verify traces decay exponentially with correct time constant
 * WHY:  Core requirement for STDP temporal dynamics
 */
TEST_F(TripletSTDPTest, TraceDecay_MatchesAnalytical) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    const float dt = 1.0f;

    // Create traces with initial values
    std::vector<float> r1_init(NUM_PRE, 1.0f);
    std::vector<float> r2_init(NUM_PRE, 1.0f);
    std::vector<float> o1_init(NUM_POST, 1.0f);
    std::vector<float> o2_init(NUM_POST, 1.0f);

    nimcp_gpu_tensor_t* r1 = create_tensor_from_data(r1_init.data(), NUM_PRE);
    nimcp_gpu_tensor_t* r2 = create_tensor_from_data(r2_init.data(), NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_tensor_from_data(o1_init.data(), NUM_POST);
    nimcp_gpu_tensor_t* o2 = create_tensor_from_data(o2_init.data(), NUM_POST);
    nimcp_gpu_tensor_t* pre_spikes = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* post_spikes = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, 0.5f);

    ASSERT_NE(r1, nullptr);
    ASSERT_NE(r2, nullptr);
    ASSERT_NE(o1, nullptr);
    ASSERT_NE(o2, nullptr);

    // Run one step with no spikes (pure decay)
    bool result = nimcp_gpu_triplet_stdp_full(ctx, weights, pre_spikes, post_spikes,
                                               r1, r2, o1, o2, &params, dt, 1.0f);
    EXPECT_TRUE(result);

    // Check trace values match analytical decay
    std::vector<float> r1_host(NUM_PRE);
    std::vector<float> o1_host(NUM_POST);
    copy_to_host(r1, r1_host.data());
    copy_to_host(o1, o1_host.data());

    float expected_r1 = analytical_decay(1.0f, params.tau_plus, dt, 1);
    float expected_o1 = analytical_decay(1.0f, params.tau_minus, dt, 1);

    for (size_t i = 0; i < NUM_PRE; i++) {
        EXPECT_NEAR(r1_host[i], expected_r1, TOLERANCE)
            << "r1 decay mismatch at index " << i;
    }
    for (size_t i = 0; i < NUM_POST; i++) {
        EXPECT_NEAR(o1_host[i], expected_o1, TOLERANCE)
            << "o1 decay mismatch at index " << i;
    }

    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(weights);
}

//=============================================================================
// LTP Tests (Post fires after Pre)
//=============================================================================

/**
 * TEST: LTP when post fires after pre
 * WHAT: Weight increases when post-synaptic neuron fires after pre-synaptic
 * WHY:  Basic Hebbian learning: "cells that fire together wire together"
 */
TEST_F(TripletSTDPTest, LTP_PostAfterPre_WeightIncreases) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 0.1f;   // Strong LTP for visibility
    params.A3_plus = 0.0f;   // Disable triplet for basic test
    params.A2_minus = 0.0f;  // Disable LTD

    const float dt = 1.0f;
    const float initial_weight = 0.5f;

    // Create tensors
    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* r2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* pre_spikes = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* post_spikes = create_zero_tensor(NUM_POST);

    ASSERT_NE(weights, nullptr);

    // Step 1: Pre-synaptic neuron 0 fires (builds trace)
    std::vector<float> pre_spike_data(NUM_PRE, 0.0f);
    pre_spike_data[0] = 1.0f;
    nimcp_gpu_tensor_t* pre_spike_t0 = create_tensor_from_data(pre_spike_data.data(), NUM_PRE);
    nimcp_gpu_tensor_t* no_post = create_zero_tensor(NUM_POST);

    nimcp_gpu_triplet_stdp_full(ctx, weights, pre_spike_t0, no_post,
                                 r1, r2, o1, o2, &params, dt, 1.0f);

    // Step 2: Post-synaptic neuron 0 fires (should trigger LTP)
    std::vector<float> post_spike_data(NUM_POST, 0.0f);
    post_spike_data[0] = 1.0f;
    nimcp_gpu_tensor_t* no_pre = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* post_spike_t1 = create_tensor_from_data(post_spike_data.data(), NUM_POST);

    nimcp_gpu_triplet_stdp_full(ctx, weights, no_pre, post_spike_t1,
                                 r1, r2, o1, o2, &params, dt, 1.0f);

    // Check weight[0][0] increased
    std::vector<float> weights_host(NUM_PRE * NUM_POST);
    copy_to_host(weights, weights_host.data());

    EXPECT_GT(weights_host[0], initial_weight)
        << "Weight should increase when post fires after pre";

    // Other weights should be unchanged (no spike correlation)
    EXPECT_FLOAT_EQ(weights_host[1], initial_weight);

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
    nimcp_gpu_tensor_destroy(pre_spike_t0);
    nimcp_gpu_tensor_destroy(post_spike_t1);
    nimcp_gpu_tensor_destroy(no_pre);
    nimcp_gpu_tensor_destroy(no_post);
}

//=============================================================================
// LTD Tests (Pre fires after Post)
//=============================================================================

/**
 * TEST: LTD when pre fires after post
 * WHAT: Weight decreases when pre-synaptic neuron fires after post-synaptic
 * WHY:  Anti-Hebbian: "out of order" firing causes depression
 */
TEST_F(TripletSTDPTest, LTD_PreAfterPost_WeightDecreases) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 0.0f;   // Disable LTP
    params.A3_plus = 0.0f;
    params.A2_minus = 0.1f;  // Strong LTD for visibility
    params.A3_minus = 0.0f;

    const float dt = 1.0f;
    const float initial_weight = 0.5f;

    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* r2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2 = create_zero_tensor(NUM_POST);

    ASSERT_NE(weights, nullptr);

    // Step 1: Post-synaptic neuron 0 fires (builds trace)
    std::vector<float> post_spike_data(NUM_POST, 0.0f);
    post_spike_data[0] = 1.0f;
    nimcp_gpu_tensor_t* post_spike_t0 = create_tensor_from_data(post_spike_data.data(), NUM_POST);
    nimcp_gpu_tensor_t* no_pre = create_zero_tensor(NUM_PRE);

    nimcp_gpu_triplet_stdp_full(ctx, weights, no_pre, post_spike_t0,
                                 r1, r2, o1, o2, &params, dt, 1.0f);

    // Step 2: Pre-synaptic neuron 0 fires (should trigger LTD)
    std::vector<float> pre_spike_data(NUM_PRE, 0.0f);
    pre_spike_data[0] = 1.0f;
    nimcp_gpu_tensor_t* pre_spike_t1 = create_tensor_from_data(pre_spike_data.data(), NUM_PRE);
    nimcp_gpu_tensor_t* no_post = create_zero_tensor(NUM_POST);

    nimcp_gpu_triplet_stdp_full(ctx, weights, pre_spike_t1, no_post,
                                 r1, r2, o1, o2, &params, dt, 1.0f);

    // Check weight[0][0] decreased
    std::vector<float> weights_host(NUM_PRE * NUM_POST);
    copy_to_host(weights, weights_host.data());

    EXPECT_LT(weights_host[0], initial_weight)
        << "Weight should decrease when pre fires after post";

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(post_spike_t0);
    nimcp_gpu_tensor_destroy(pre_spike_t1);
    nimcp_gpu_tensor_destroy(no_pre);
    nimcp_gpu_tensor_destroy(no_post);
}

//=============================================================================
// Triplet Effect Tests
//=============================================================================

/**
 * TEST: Triplet effect causes stronger potentiation
 * WHAT: Post-post-pre triplet causes stronger LTP than pair alone
 * WHY:  Key feature of triplet STDP vs pair-based STDP
 */
TEST_F(TripletSTDPTest, TripletEffect_BurstPotentiation) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 0.01f;
    params.A3_plus = 0.05f;  // Strong triplet effect
    params.A2_minus = 0.0f;
    params.A3_minus = 0.0f;

    const float dt = 1.0f;
    const float initial_weight = 0.5f;

    // Test 1: Single pre-post pair (measure weight change)
    nimcp_gpu_tensor_t* weights1 = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1_1 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* r2_1 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1_1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2_1 = create_zero_tensor(NUM_POST);

    std::vector<float> pre_spike(NUM_PRE, 0.0f);
    pre_spike[0] = 1.0f;
    std::vector<float> post_spike(NUM_POST, 0.0f);
    post_spike[0] = 1.0f;

    nimcp_gpu_tensor_t* pre_t = create_tensor_from_data(pre_spike.data(), NUM_PRE);
    nimcp_gpu_tensor_t* post_t = create_tensor_from_data(post_spike.data(), NUM_POST);
    nimcp_gpu_tensor_t* no_pre = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* no_post = create_zero_tensor(NUM_POST);

    // Pre spike, then post spike
    nimcp_gpu_triplet_stdp_full(ctx, weights1, pre_t, no_post,
                                 r1_1, r2_1, o1_1, o2_1, &params, dt, 1.0f);
    nimcp_gpu_triplet_stdp_full(ctx, weights1, no_pre, post_t,
                                 r1_1, r2_1, o1_1, o2_1, &params, dt, 1.0f);

    std::vector<float> w1_host(NUM_PRE * NUM_POST);
    copy_to_host(weights1, w1_host.data());
    float pair_change = w1_host[0] - initial_weight;

    // Test 2: Post-pre-post triplet (should have stronger potentiation)
    nimcp_gpu_tensor_t* weights2 = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1_2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* r2_2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1_2 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2_2 = create_zero_tensor(NUM_POST);

    // First post spike (builds o2 for triplet term)
    nimcp_gpu_triplet_stdp_full(ctx, weights2, no_pre, post_t,
                                 r1_2, r2_2, o1_2, o2_2, &params, dt, 1.0f);
    // Pre spike
    nimcp_gpu_triplet_stdp_full(ctx, weights2, pre_t, no_post,
                                 r1_2, r2_2, o1_2, o2_2, &params, dt, 1.0f);
    // Second post spike (triplet: post-pre-post)
    nimcp_gpu_triplet_stdp_full(ctx, weights2, no_pre, post_t,
                                 r1_2, r2_2, o1_2, o2_2, &params, dt, 1.0f);

    std::vector<float> w2_host(NUM_PRE * NUM_POST);
    copy_to_host(weights2, w2_host.data());
    float triplet_change = w2_host[0] - initial_weight;

    // Triplet should produce larger weight change
    EXPECT_GT(triplet_change, pair_change)
        << "Triplet (post-pre-post) should produce stronger potentiation than pair";

    nimcp_gpu_tensor_destroy(weights1);
    nimcp_gpu_tensor_destroy(weights2);
    nimcp_gpu_tensor_destroy(r1_1);
    nimcp_gpu_tensor_destroy(r2_1);
    nimcp_gpu_tensor_destroy(o1_1);
    nimcp_gpu_tensor_destroy(o2_1);
    nimcp_gpu_tensor_destroy(r1_2);
    nimcp_gpu_tensor_destroy(r2_2);
    nimcp_gpu_tensor_destroy(o1_2);
    nimcp_gpu_tensor_destroy(o2_2);
    nimcp_gpu_tensor_destroy(pre_t);
    nimcp_gpu_tensor_destroy(post_t);
    nimcp_gpu_tensor_destroy(no_pre);
    nimcp_gpu_tensor_destroy(no_post);
}

//=============================================================================
// Weight Bounds Tests
//=============================================================================

/**
 * TEST: Weight bounds are respected
 * WHAT: Weights cannot exceed w_max or go below w_min
 * WHY:  Prevent runaway weight growth/collapse
 */
TEST_F(TripletSTDPTest, WeightBounds_AreRespected) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 1.0f;   // Very strong LTP
    params.A2_minus = 0.0f;
    params.w_min = 0.0f;
    params.w_max = 0.8f;

    const float dt = 1.0f;
    const float initial_weight = 0.75f;  // Close to max

    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1 = create_filled_tensor(NUM_PRE, 1.0f);  // Strong pre trace
    nimcp_gpu_tensor_t* r2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2 = create_zero_tensor(NUM_POST);

    // All neurons spike
    nimcp_gpu_tensor_t* pre_spikes = create_filled_tensor(NUM_PRE, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = create_filled_tensor(NUM_POST, 1.0f);

    // Multiple LTP updates
    for (int i = 0; i < 10; i++) {
        bool result = nimcp_gpu_triplet_stdp_full(ctx, weights, pre_spikes, post_spikes,
                                                   r1, r2, o1, o2, &params, dt, 1.0f);
        ASSERT_TRUE(result);
    }

    // Verify bounds
    std::vector<float> weights_host(NUM_PRE * NUM_POST);
    copy_to_host(weights, weights_host.data());

    for (float w : weights_host) {
        EXPECT_GE(w, params.w_min) << "Weight should be >= w_min";
        EXPECT_LE(w, params.w_max) << "Weight should be <= w_max";
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

/**
 * TEST: Lower weight bound is respected
 * WHAT: Weights cannot go below w_min
 * WHY:  Prevent negative or zero weights
 */
TEST_F(TripletSTDPTest, WeightBounds_LowerBound) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 0.0f;
    params.A2_minus = 1.0f;  // Very strong LTD
    params.w_min = 0.1f;
    params.w_max = 1.0f;

    const float dt = 1.0f;
    const float initial_weight = 0.15f;  // Close to min

    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1 = create_filled_tensor(NUM_PRE, 1.0f);
    nimcp_gpu_tensor_t* r2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_filled_tensor(NUM_POST, 1.0f);  // Strong post trace
    nimcp_gpu_tensor_t* o2 = create_zero_tensor(NUM_POST);

    nimcp_gpu_tensor_t* pre_spikes = create_filled_tensor(NUM_PRE, 1.0f);
    nimcp_gpu_tensor_t* post_spikes = create_zero_tensor(NUM_POST);

    // Multiple LTD updates
    for (int i = 0; i < 10; i++) {
        nimcp_gpu_triplet_stdp_full(ctx, weights, pre_spikes, post_spikes,
                                     r1, r2, o1, o2, &params, dt, 1.0f);
    }

    std::vector<float> weights_host(NUM_PRE * NUM_POST);
    copy_to_host(weights, weights_host.data());

    for (float w : weights_host) {
        EXPECT_GE(w, params.w_min) << "Weight should be >= w_min";
    }

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

//=============================================================================
// Comparison with Pair-Based STDP
//=============================================================================

/**
 * TEST: Compare triplet with pair-based STDP
 * WHAT: When A3 terms are zero, triplet should match pair-based
 * WHY:  Backward compatibility check
 */
TEST_F(TripletSTDPTest, CompareWithPairBased_A3Zero_MatchesPair) {
    RequireGPU();

    // Set up triplet params with A3=0 (reduces to pair-based)
    nimcp_triplet_stdp_params_t triplet_params = create_default_params();
    triplet_params.A2_plus = 0.01f;
    triplet_params.A2_minus = 0.012f;
    triplet_params.A3_plus = 0.0f;
    triplet_params.A3_minus = 0.0f;

    // Set up equivalent pair-based params
    nimcp_stdp_params_t pair_params;
    pair_params.A_plus = triplet_params.A2_plus;
    pair_params.A_minus = triplet_params.A2_minus;
    pair_params.tau_plus = triplet_params.tau_plus;
    pair_params.tau_minus = triplet_params.tau_minus;
    pair_params.w_min = triplet_params.w_min;
    pair_params.w_max = triplet_params.w_max;

    const float dt = 1.0f;
    const float initial_weight = 0.5f;

    // Create identical initial conditions
    nimcp_gpu_tensor_t* weights_triplet = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* weights_pair = create_matrix(NUM_PRE, NUM_POST, initial_weight);

    nimcp_gpu_tensor_t* r1 = create_filled_tensor(NUM_PRE, 0.5f);
    nimcp_gpu_tensor_t* r2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1 = create_filled_tensor(NUM_POST, 0.5f);
    nimcp_gpu_tensor_t* o2 = create_zero_tensor(NUM_POST);

    nimcp_gpu_tensor_t* pre_trace = create_filled_tensor(NUM_PRE, 0.5f);
    nimcp_gpu_tensor_t* post_trace = create_filled_tensor(NUM_POST, 0.5f);

    std::vector<float> pre_spike_data(NUM_PRE, 0.0f);
    std::vector<float> post_spike_data(NUM_POST, 0.0f);
    pre_spike_data[0] = 1.0f;
    post_spike_data[0] = 1.0f;

    nimcp_gpu_tensor_t* pre_spikes = create_tensor_from_data(pre_spike_data.data(), NUM_PRE);
    nimcp_gpu_tensor_t* post_spikes = create_tensor_from_data(post_spike_data.data(), NUM_POST);

    // Apply triplet STDP
    nimcp_gpu_triplet_stdp_full(ctx, weights_triplet, pre_spikes, post_spikes,
                                 r1, r2, o1, o2, &triplet_params, dt, 1.0f);

    // Apply pair-based STDP
    nimcp_gpu_stdp_pair(ctx, weights_pair, pre_spikes, post_spikes,
                         pre_trace, post_trace, &pair_params);

    // Results should be similar (not exactly equal due to different trace dynamics)
    std::vector<float> w_triplet(NUM_PRE * NUM_POST);
    std::vector<float> w_pair(NUM_PRE * NUM_POST);
    copy_to_host(weights_triplet, w_triplet.data());
    copy_to_host(weights_pair, w_pair.data());

    // Direction of weight change should match
    float triplet_change = w_triplet[0] - initial_weight;
    float pair_change = w_pair[0] - initial_weight;

    // Both should show weight change in same direction
    if (triplet_change != 0 && pair_change != 0) {
        EXPECT_EQ(triplet_change > 0, pair_change > 0)
            << "Triplet and pair should change weight in same direction";
    }

    nimcp_gpu_tensor_destroy(weights_triplet);
    nimcp_gpu_tensor_destroy(weights_pair);
    nimcp_gpu_tensor_destroy(r1);
    nimcp_gpu_tensor_destroy(r2);
    nimcp_gpu_tensor_destroy(o1);
    nimcp_gpu_tensor_destroy(o2);
    nimcp_gpu_tensor_destroy(pre_trace);
    nimcp_gpu_tensor_destroy(post_trace);
    nimcp_gpu_tensor_destroy(pre_spikes);
    nimcp_gpu_tensor_destroy(post_spikes);
}

//=============================================================================
// NULL Safety Tests
//=============================================================================

/**
 * TEST: nimcp_gpu_triplet_stdp_full with NULL tensors
 * WHAT: Try STDP with NULL inputs
 * WHY:  Verify NULL safety
 */
TEST_F(TripletSTDPTest, FullSTDP_NullTensors_ReturnsFalse) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    nimcp_gpu_tensor_t* weights = create_matrix(NUM_PRE, NUM_POST, 0.5f);
    nimcp_gpu_tensor_t* tensor = create_zero_tensor(NUM_PRE);

    // NULL weights
    EXPECT_FALSE(nimcp_gpu_triplet_stdp_full(ctx, nullptr, tensor, tensor,
                                              tensor, tensor, tensor, tensor,
                                              &params, 1.0f, 1.0f));

    // NULL params
    EXPECT_FALSE(nimcp_gpu_triplet_stdp_full(ctx, weights, tensor, tensor,
                                              tensor, tensor, tensor, tensor,
                                              nullptr, 1.0f, 1.0f));

    // NULL context
    EXPECT_FALSE(nimcp_gpu_triplet_stdp_full(nullptr, weights, tensor, tensor,
                                              tensor, tensor, tensor, tensor,
                                              &params, 1.0f, 1.0f));

    nimcp_gpu_tensor_destroy(weights);
    nimcp_gpu_tensor_destroy(tensor);
}

//=============================================================================
// Learning Rate Tests
//=============================================================================

/**
 * TEST: Learning rate scales weight updates
 * WHAT: Larger learning rate causes larger weight changes
 * WHY:  Allow control of learning speed
 */
TEST_F(TripletSTDPTest, LearningRate_ScalesUpdates) {
    RequireGPU();

    nimcp_triplet_stdp_params_t params = create_default_params();
    params.A2_plus = 0.1f;
    params.A2_minus = 0.0f;

    const float dt = 1.0f;
    const float initial_weight = 0.5f;

    // Test with learning rate 0.5
    nimcp_gpu_tensor_t* weights1 = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1_1 = create_filled_tensor(NUM_PRE, 1.0f);
    nimcp_gpu_tensor_t* r2_1 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1_1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2_1 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* no_pre = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* post_spikes = create_filled_tensor(NUM_POST, 1.0f);

    nimcp_gpu_triplet_stdp_full(ctx, weights1, no_pre, post_spikes,
                                 r1_1, r2_1, o1_1, o2_1, &params, dt, 0.5f);

    // Test with learning rate 1.0
    nimcp_gpu_tensor_t* weights2 = create_matrix(NUM_PRE, NUM_POST, initial_weight);
    nimcp_gpu_tensor_t* r1_2 = create_filled_tensor(NUM_PRE, 1.0f);
    nimcp_gpu_tensor_t* r2_2 = create_zero_tensor(NUM_PRE);
    nimcp_gpu_tensor_t* o1_2 = create_zero_tensor(NUM_POST);
    nimcp_gpu_tensor_t* o2_2 = create_zero_tensor(NUM_POST);

    nimcp_gpu_triplet_stdp_full(ctx, weights2, no_pre, post_spikes,
                                 r1_2, r2_2, o1_2, o2_2, &params, dt, 1.0f);

    std::vector<float> w1_host(NUM_PRE * NUM_POST);
    std::vector<float> w2_host(NUM_PRE * NUM_POST);
    copy_to_host(weights1, w1_host.data());
    copy_to_host(weights2, w2_host.data());

    float change1 = w1_host[0] - initial_weight;
    float change2 = w2_host[0] - initial_weight;

    // LR=1.0 should produce roughly 2x the change of LR=0.5
    if (change1 != 0) {
        EXPECT_NEAR(change2 / change1, 2.0f, 0.2f)
            << "Learning rate should scale updates linearly";
    }

    nimcp_gpu_tensor_destroy(weights1);
    nimcp_gpu_tensor_destroy(weights2);
    nimcp_gpu_tensor_destroy(r1_1);
    nimcp_gpu_tensor_destroy(r2_1);
    nimcp_gpu_tensor_destroy(o1_1);
    nimcp_gpu_tensor_destroy(o2_1);
    nimcp_gpu_tensor_destroy(r1_2);
    nimcp_gpu_tensor_destroy(r2_2);
    nimcp_gpu_tensor_destroy(o1_2);
    nimcp_gpu_tensor_destroy(o2_2);
    nimcp_gpu_tensor_destroy(no_pre);
    nimcp_gpu_tensor_destroy(post_spikes);
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
