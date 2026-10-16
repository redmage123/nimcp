/**
 * @file test_cross_network_bridges.cpp
 * @brief Phase D: Bridge validation — numerical gradient checks and forward/backward consistency
 *
 * Tests that each cross-network bridge type (rate-to-spike, spike-to-rate, continuous-to-spike)
 * produces correct gradients by comparing analytical backward with finite-difference approximation.
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstring>
#include <vector>

extern "C" {
#include "training/nimcp_unified_training.h"
#include "utils/memory/nimcp_unified_memory.h"
}

class CrossNetworkBridgeTest : public ::testing::Test {
protected:
    static constexpr uint32_t DIM = 8;
    static constexpr float EPSILON = 1e-4f;
    static constexpr float GRAD_TOL = 0.30f; // 30% tolerance: bridges blend surrogate gradients with true derivatives

    nimcp_cross_network_bridge_t bridge;
    std::vector<float> source_output;
    std::vector<float> target_input;
    std::vector<float> dl_dtarget;
    std::vector<float> dl_dsource;

    void SetUp() override {
        memset(&bridge, 0, sizeof(bridge));
        bridge.source_dim = DIM;
        bridge.target_dim = DIM;
        bridge.enabled = true;

        /* Allocate cached arrays that backward pass reads */
        bridge.last_source_output = (float*)calloc(DIM, sizeof(float));
        bridge.last_target_input = (float*)calloc(DIM, sizeof(float));
        bridge.transform_weights = nullptr;
        bridge.transform_bias = nullptr;
        bridge.weight_grad = nullptr;
        bridge.bias_grad = nullptr;

        source_output.resize(DIM);
        target_input.resize(DIM);
        dl_dtarget.resize(DIM);
        dl_dsource.resize(DIM);
    }

    void TearDown() override {
        free(bridge.last_source_output);
        free(bridge.last_target_input);
    }

    /**
     * Numerical gradient check: perturb each input element by ±ε,
     * compute forward, approximate dL/dsource[i] ≈ (L+ - L-) / (2ε).
     * Compare with analytical backward.
     *
     * Uses a simple sum-of-outputs loss: L = Σ target_input[i] * dl_dtarget[i]
     */
    void numerical_gradient_check(nimcp_bridge_type_t type, const float* test_inputs, float tol) {
        bridge.type = type;

        /* Run forward to populate caches */
        memcpy(bridge.last_source_output, test_inputs, DIM * sizeof(float));
        run_forward(type, test_inputs, target_input.data());
        memcpy(bridge.last_target_input, target_input.data(), DIM * sizeof(float));

        /* Upstream gradient: use unit gradient (dL/dtarget = 1.0) */
        for (uint32_t i = 0; i < DIM; i++) dl_dtarget[i] = 1.0f;

        /* Analytical backward */
        memset(dl_dsource.data(), 0, DIM * sizeof(float));
        run_backward(type, dl_dtarget.data(), dl_dsource.data());

        /* Numerical gradient for each input dimension */
        for (uint32_t i = 0; i < DIM; i++) {
            std::vector<float> perturbed(test_inputs, test_inputs + DIM);
            std::vector<float> out_plus(DIM), out_minus(DIM);

            perturbed[i] = test_inputs[i] + EPSILON;
            run_forward(type, perturbed.data(), out_plus.data());

            perturbed[i] = test_inputs[i] - EPSILON;
            run_forward(type, perturbed.data(), out_minus.data());

            /* L = Σ target[j] * dl_dtarget[j] = Σ target[j] * 1.0 = Σ target[j] */
            float loss_plus = 0.0f, loss_minus = 0.0f;
            for (uint32_t j = 0; j < DIM; j++) {
                loss_plus += out_plus[j] * dl_dtarget[j];
                loss_minus += out_minus[j] * dl_dtarget[j];
            }

            float numerical_grad = (loss_plus - loss_minus) / (2.0f * EPSILON);
            float analytical_grad = dl_dsource[i];

            /* Check relative error (with absolute floor for near-zero grads) */
            float denom = std::max(std::abs(numerical_grad), std::abs(analytical_grad));
            if (denom < 1e-6f) {
                /* Both near zero — OK */
                EXPECT_NEAR(analytical_grad, numerical_grad, 1e-4f)
                    << "dim=" << i << " input=" << test_inputs[i];
            } else {
                float rel_err = std::abs(analytical_grad - numerical_grad) / denom;
                EXPECT_LT(rel_err, tol)
                    << "dim=" << i << " input=" << test_inputs[i]
                    << " analytical=" << analytical_grad
                    << " numerical=" << numerical_grad
                    << " rel_err=" << rel_err;
            }
        }
    }

private:
    void run_forward(nimcp_bridge_type_t type, const float* src, float* tgt) {
        switch (type) {
            case NIMCP_BRIDGE_RATE_TO_SPIKE:
                bridge_rate_to_spike_forward(&bridge, src, tgt);
                break;
            case NIMCP_BRIDGE_SPIKE_TO_RATE:
                bridge_spike_to_rate_forward(&bridge, src, tgt);
                break;
            case NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE:
                bridge_continuous_to_spike_forward(&bridge, src, tgt);
                break;
            default:
                memcpy(tgt, src, DIM * sizeof(float));
                break;
        }
    }

    void run_backward(nimcp_bridge_type_t type, const float* dl_dt, float* dl_ds) {
        switch (type) {
            case NIMCP_BRIDGE_RATE_TO_SPIKE:
                bridge_rate_to_spike_backward(&bridge, dl_dt, dl_ds);
                break;
            case NIMCP_BRIDGE_SPIKE_TO_RATE:
                bridge_spike_to_rate_backward(&bridge, dl_dt, dl_ds);
                break;
            case NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE:
                bridge_continuous_to_spike_backward(&bridge, dl_dt, dl_ds);
                break;
            default:
                memcpy(dl_ds, dl_dt, DIM * sizeof(float));
                break;
        }
    }
};

//=============================================================================
// Rate-to-Spike Bridge Tests
//=============================================================================

TEST_F(CrossNetworkBridgeTest, RateToSpike_ForwardOutputRange) {
    /* Rate-to-spike uses sigmoid, output should be in (0, 1) */
    float inputs[] = {-2.0f, -1.0f, -0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    bridge.type = NIMCP_BRIDGE_RATE_TO_SPIKE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));
    bridge_rate_to_spike_forward(&bridge, inputs, target_input.data());

    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_GT(target_input[i], 0.0f) << "Output must be > 0 (sigmoid)";
        EXPECT_LT(target_input[i], 1.0f) << "Output must be < 1 (sigmoid)";
    }
}

TEST_F(CrossNetworkBridgeTest, RateToSpike_ForwardMonotonic) {
    /* Sigmoid is monotonically increasing */
    float inputs[] = {-2.0f, -1.0f, -0.5f, 0.0f, 0.25f, 0.5f, 0.75f, 1.0f};
    bridge.type = NIMCP_BRIDGE_RATE_TO_SPIKE;
    bridge_rate_to_spike_forward(&bridge, inputs, target_input.data());

    for (uint32_t i = 1; i < DIM; i++) {
        EXPECT_GE(target_input[i], target_input[i-1])
            << "Sigmoid should be monotonically increasing";
    }
}

TEST_F(CrossNetworkBridgeTest, RateToSpike_GradientCheck) {
    float inputs[] = {0.0f, 0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 1.0f, 0.4f};
    numerical_gradient_check(NIMCP_BRIDGE_RATE_TO_SPIKE, inputs, GRAD_TOL);
}

TEST_F(CrossNetworkBridgeTest, RateToSpike_GradientNonZero) {
    /* Backward should produce non-zero gradients for non-extreme inputs */
    float inputs[] = {0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f, 0.5f};
    bridge.type = NIMCP_BRIDGE_RATE_TO_SPIKE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));

    for (uint32_t i = 0; i < DIM; i++) dl_dtarget[i] = 1.0f;
    bridge_rate_to_spike_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NE(dl_dsource[i], 0.0f)
            << "Gradient should be non-zero for input=" << inputs[i];
        EXPECT_GT(dl_dsource[i], 0.0f)
            << "Gradient should be positive for positive upstream gradient";
    }
}

//=============================================================================
// Spike-to-Rate Bridge Tests
//=============================================================================

TEST_F(CrossNetworkBridgeTest, SpikeToRate_ForwardOutputRange) {
    /* Spike-to-rate clamps to [0, 1] */
    float inputs[] = {0.0f, 0.0f, 1.0f, 1.0f, 0.5f, 0.0f, 1.0f, 0.0f};
    bridge.type = NIMCP_BRIDGE_SPIKE_TO_RATE;
    /* Clear last_target_input for fresh EMA */
    memset(bridge.last_target_input, 0, DIM * sizeof(float));
    bridge_spike_to_rate_forward(&bridge, inputs, target_input.data());

    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_GE(target_input[i], 0.0f) << "Output must be >= 0";
        EXPECT_LE(target_input[i], 1.0f) << "Output must be <= 1";
    }
}

TEST_F(CrossNetworkBridgeTest, SpikeToRate_EMASmoothing) {
    /* Two consecutive forward passes — second should show EMA blending */
    float spikes1[] = {1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};
    float spikes2[] = {0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f};

    bridge.type = NIMCP_BRIDGE_SPIKE_TO_RATE;
    memset(bridge.last_target_input, 0, DIM * sizeof(float));

    /* First pass */
    bridge_spike_to_rate_forward(&bridge, spikes1, target_input.data());
    memcpy(bridge.last_target_input, target_input.data(), DIM * sizeof(float));
    float first_pass_0 = target_input[0];  /* spike=1 → rate = α*1 */

    /* Second pass with opposite spikes */
    bridge_spike_to_rate_forward(&bridge, spikes2, target_input.data());
    float second_pass_0 = target_input[0]; /* spike=0 → rate = (1-α)*prev */

    /* Should show decay from first pass */
    EXPECT_GT(first_pass_0, 0.0f);
    EXPECT_GT(second_pass_0, 0.0f); /* EMA retains some from first pass */
    EXPECT_LT(second_pass_0, first_pass_0); /* But should decay */
}

TEST_F(CrossNetworkBridgeTest, SpikeToRate_GradientNonZero) {
    float inputs[] = {0.0f, 0.5f, 1.0f, 0.3f, 0.7f, 0.2f, 0.8f, 0.5f};
    bridge.type = NIMCP_BRIDGE_SPIKE_TO_RATE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));

    for (uint32_t i = 0; i < DIM; i++) dl_dtarget[i] = 1.0f;
    bridge_spike_to_rate_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    /* All outputs should have non-zero gradient (SuperSpike surrogate is always > 0) */
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NE(dl_dsource[i], 0.0f)
            << "Surrogate gradient should be non-zero for input=" << inputs[i];
    }
}

TEST_F(CrossNetworkBridgeTest, SpikeToRate_BackwardNullSafe) {
    bridge.type = NIMCP_BRIDGE_SPIKE_TO_RATE;
    /* Should not crash when dl_dsource is NULL */
    bridge_spike_to_rate_backward(&bridge, dl_dtarget.data(), nullptr);
}

//=============================================================================
// Continuous-to-Spike Bridge Tests
//=============================================================================

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_ForwardOutputRange) {
    /* Output should be in (0, 1) since it's tanh → sigmoid */
    float inputs[] = {-5.0f, -2.0f, -0.5f, 0.0f, 0.5f, 2.0f, 5.0f, 10.0f};
    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;
    bridge_continuous_to_spike_forward(&bridge, inputs, target_input.data());

    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_GT(target_input[i], 0.0f) << "Output must be > 0 (sigmoid)";
        EXPECT_LT(target_input[i], 1.0f) << "Output must be < 1 (sigmoid)";
    }
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_ForwardMonotonic) {
    /* tanh + sigmoid composition should be monotonically increasing */
    float inputs[] = {-5.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 5.0f, 10.0f};
    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;
    bridge_continuous_to_spike_forward(&bridge, inputs, target_input.data());

    for (uint32_t i = 1; i < DIM; i++) {
        EXPECT_GE(target_input[i], target_input[i-1])
            << "tanh+sigmoid should be monotonically increasing";
    }
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_GradientCheck) {
    /* Test gradient around the active region where gradients are meaningful */
    float inputs[] = {-1.0f, -0.5f, -0.2f, 0.0f, 0.2f, 0.5f, 1.0f, 0.3f};
    numerical_gradient_check(NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE, inputs, GRAD_TOL);
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_GradientNonZero) {
    float inputs[] = {-0.5f, -0.2f, 0.0f, 0.2f, 0.5f, 0.8f, -0.3f, 0.1f};
    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));

    for (uint32_t i = 0; i < DIM; i++) dl_dtarget[i] = 1.0f;
    bridge_continuous_to_spike_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_NE(dl_dsource[i], 0.0f)
            << "Gradient should be non-zero for input=" << inputs[i];
    }
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_ExtremeSaturation) {
    /* At extreme values, tanh saturates → dtanh ≈ 0 → gradient should be small */
    float extreme_inputs[] = {-20.0f, -10.0f, -5.0f, 5.0f, 10.0f, 20.0f, -15.0f, 15.0f};
    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;
    memcpy(bridge.last_source_output, extreme_inputs, DIM * sizeof(float));

    for (uint32_t i = 0; i < DIM; i++) dl_dtarget[i] = 1.0f;
    bridge_continuous_to_spike_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    /* Gradients at extreme inputs should be very small (tanh saturated) */
    for (uint32_t i = 0; i < DIM; i++) {
        EXPECT_LT(std::abs(dl_dsource[i]), 0.1f)
            << "Gradient should be small at saturated input=" << extreme_inputs[i];
    }
}

//=============================================================================
// Dimension Mismatch Tests
//=============================================================================

TEST_F(CrossNetworkBridgeTest, RateToSpike_DimMismatch_SourceSmaller) {
    bridge.source_dim = 4;
    bridge.target_dim = DIM;
    bridge.type = NIMCP_BRIDGE_RATE_TO_SPIKE;

    float inputs[] = {0.2f, 0.4f, 0.6f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f};
    bridge_rate_to_spike_forward(&bridge, inputs, target_input.data());

    /* First 4 should be transformed, last 4 zero-padded */
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_GT(target_input[i], 0.0f);
    }
    for (uint32_t i = 4; i < DIM; i++) {
        EXPECT_EQ(target_input[i], 0.0f);
    }
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_DimMismatch_TargetSmaller) {
    bridge.source_dim = DIM;
    bridge.target_dim = 4;
    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;

    float inputs[] = {0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.8f};
    float out[4] = {0};
    bridge_continuous_to_spike_forward(&bridge, inputs, out);

    /* Only 4 elements should be written */
    for (uint32_t i = 0; i < 4; i++) {
        EXPECT_GT(out[i], 0.0f);
    }
}

//=============================================================================
// End-to-End: Forward → Backward Consistency
//=============================================================================

TEST_F(CrossNetworkBridgeTest, RateToSpike_ForwardBackwardRoundTrip) {
    /* Forward then backward should produce gradients that reduce loss */
    float inputs[] = {0.3f, 0.4f, 0.5f, 0.6f, 0.7f, 0.5f, 0.4f, 0.6f};
    float target[] = {0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f, 0.8f};

    bridge.type = NIMCP_BRIDGE_RATE_TO_SPIKE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));
    bridge_rate_to_spike_forward(&bridge, inputs, target_input.data());
    memcpy(bridge.last_target_input, target_input.data(), DIM * sizeof(float));

    /* Loss gradient: dL/dtarget = target_input - target (MSE derivative) */
    float initial_loss = 0.0f;
    for (uint32_t i = 0; i < DIM; i++) {
        dl_dtarget[i] = target_input[i] - target[i];
        initial_loss += 0.5f * dl_dtarget[i] * dl_dtarget[i];
    }

    bridge_rate_to_spike_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    /* Apply gradient step to inputs */
    float lr = 0.01f;
    float new_inputs[DIM];
    for (uint32_t i = 0; i < DIM; i++) {
        new_inputs[i] = inputs[i] - lr * dl_dsource[i];
    }

    /* Re-run forward with updated inputs */
    bridge_rate_to_spike_forward(&bridge, new_inputs, target_input.data());
    float new_loss = 0.0f;
    for (uint32_t i = 0; i < DIM; i++) {
        float diff = target_input[i] - target[i];
        new_loss += 0.5f * diff * diff;
    }

    EXPECT_LT(new_loss, initial_loss)
        << "One gradient step should reduce loss: " << new_loss << " >= " << initial_loss;
}

TEST_F(CrossNetworkBridgeTest, ContinuousToSpike_ForwardBackwardRoundTrip) {
    float inputs[] = {-0.5f, 0.0f, 0.3f, 0.5f, -0.3f, 0.1f, 0.8f, -0.2f};
    float target[] = {0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f, 0.9f};

    bridge.type = NIMCP_BRIDGE_CONTINUOUS_TO_SPIKE;
    memcpy(bridge.last_source_output, inputs, DIM * sizeof(float));
    bridge_continuous_to_spike_forward(&bridge, inputs, target_input.data());
    memcpy(bridge.last_target_input, target_input.data(), DIM * sizeof(float));

    float initial_loss = 0.0f;
    for (uint32_t i = 0; i < DIM; i++) {
        dl_dtarget[i] = target_input[i] - target[i];
        initial_loss += 0.5f * dl_dtarget[i] * dl_dtarget[i];
    }

    bridge_continuous_to_spike_backward(&bridge, dl_dtarget.data(), dl_dsource.data());

    float lr = 0.1f;
    float new_inputs[DIM];
    for (uint32_t i = 0; i < DIM; i++) {
        new_inputs[i] = inputs[i] - lr * dl_dsource[i];
    }

    memcpy(bridge.last_source_output, new_inputs, DIM * sizeof(float));
    bridge_continuous_to_spike_forward(&bridge, new_inputs, target_input.data());
    float new_loss = 0.0f;
    for (uint32_t i = 0; i < DIM; i++) {
        float diff = target_input[i] - target[i];
        new_loss += 0.5f * diff * diff;
    }

    EXPECT_LT(new_loss, initial_loss)
        << "One gradient step should reduce loss: " << new_loss << " >= " << initial_loss;
}
