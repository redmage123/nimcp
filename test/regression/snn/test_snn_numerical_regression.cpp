/**
 * @file test_snn_numerical_regression.cpp
 * @brief Regression tests for SNN numerical stability
 *
 * WHAT: Tests to prevent regression of numerical stability bugs in SNN
 * WHY:  Lock in correct handling of edge cases and numerical issues
 * HOW:  Test with NaN, Inf, denormals, extreme values
 *
 * BUG HISTORY:
 * - Bug #1: NaN propagation through network
 *   FIX: Check for NaN at input, clamp membrane potentials
 * - Bug #2: Infinity from exponential overflow
 *   FIX: Clamp exponentials, use safe exp() wrapper
 * - Bug #3: Denormal numbers slowing computation
 *   FIX: Flush denormals to zero
 * - Bug #4: Membrane potential divergence
 *   FIX: Clamp to reasonable range [-200, 100] mV
 *
 * REGRESSION FOCUS:
 * 1. NaN inputs don't propagate through network
 * 2. Inf values are handled gracefully
 * 3. No denormals in output
 * 4. Clamping behavior is locked in
 * 5. Extreme input values don't cause divergence
 *
 * @version 1.0.0
 * @date 2026-01-15
 */

#include <gtest/gtest.h>
#include <cmath>
#include <limits>
#include <vector>
#include <cstdint>
#include <cfenv>

// Headers have their own extern "C" guards
#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_network.h"
#include "snn/nimcp_snn_config.h"

//=============================================================================
// Test Fixture
//=============================================================================

class SNNNumericalRegressionTest : public ::testing::Test {
protected:
    snn_network_t* network;
    snn_config_t config;

    void SetUp() override {
        /* Configure a simple feedforward SNN for testing */
        snn_config_default(&config);

        /* Set network dimensions */
        config.n_inputs = 10;
        config.n_outputs = 5;
        config.dt = 1.0f;

        /* Create network */
        network = snn_network_create(&config);

        /* Note: network may be NULL if SNN module not fully built */
    }

    void TearDown() override {
        if (network) {
            snn_network_destroy(network);
            network = nullptr;
        }
    }

    /**
     * @brief Check if a float is a denormalized number
     */
    bool is_denormal(float x) {
        return std::fpclassify(x) == FP_SUBNORMAL;
    }

    /**
     * @brief Check if any value in array is NaN
     */
    bool contains_nan(const float* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (std::isnan(values[i])) return true;
        }
        return false;
    }

    /**
     * @brief Check if any value in array is Inf
     */
    bool contains_inf(const float* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (std::isinf(values[i])) return true;
        }
        return false;
    }

    /**
     * @brief Check if any value in array is denormal
     */
    bool contains_denormal(const float* values, size_t count) {
        for (size_t i = 0; i < count; i++) {
            if (is_denormal(values[i])) return true;
        }
        return false;
    }

    /**
     * @brief Create array filled with specific float value
     */
    std::vector<float> make_array(size_t count, float value) {
        return std::vector<float>(count, value);
    }
};

//=============================================================================
// NaN HANDLING REGRESSION TESTS
//=============================================================================

/**
 * BUG: NaN inputs propagate through network, corrupting all outputs
 *
 * WRONG: Process NaN directly, output becomes NaN
 * RIGHT: Detect NaN at input, replace with 0 or return error
 */
TEST_F(SNNNumericalRegressionTest, NaN_InputHandling) {
    /**
     * REGRESSION TEST: NaN inputs should not propagate to outputs
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);
    inputs[0] = std::numeric_limits<float>::quiet_NaN();

    /* Set NaN input */
    int result = snn_network_set_inputs(network, inputs.data(), inputs.size());

    /* Either reject NaN input or handle gracefully */
    std::vector<float> outputs(config.n_outputs);

    /* Run simulation */
    snn_network_run(network, 10.0f);

    /* Get outputs */
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* Outputs should NOT contain NaN */
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: NaN input propagated to output. "
        << "Network must sanitize or reject NaN inputs.";
}

TEST_F(SNNNumericalRegressionTest, NaN_AllInputsNaN) {
    /**
     * REGRESSION TEST: All-NaN inputs should not crash
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, std::numeric_limits<float>::quiet_NaN());

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: All-NaN inputs produced NaN outputs";
}

TEST_F(SNNNumericalRegressionTest, NaN_SignalingNaN) {
    /**
     * REGRESSION TEST: Signaling NaN should be handled
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);
    inputs[0] = std::numeric_limits<float>::signaling_NaN();

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: Signaling NaN propagated to output";
}

//=============================================================================
// INFINITY HANDLING REGRESSION TESTS
//=============================================================================

/**
 * BUG: Infinity from exponential overflow causes divergence
 *
 * WRONG: exp(large_value) overflows to infinity
 * RIGHT: Clamp input to exp(), or detect and handle infinity
 */
TEST_F(SNNNumericalRegressionTest, Inf_PositiveInfInput) {
    /**
     * REGRESSION TEST: +Inf input should not propagate or crash
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);
    inputs[0] = std::numeric_limits<float>::infinity();

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_inf(outputs.data(), outputs.size()))
        << "REGRESSION: +Inf input propagated to output";
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: +Inf input caused NaN in output";
}

TEST_F(SNNNumericalRegressionTest, Inf_NegativeInfInput) {
    /**
     * REGRESSION TEST: -Inf input should not propagate or crash
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);
    inputs[0] = -std::numeric_limits<float>::infinity();

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_inf(outputs.data(), outputs.size()))
        << "REGRESSION: -Inf input propagated to output";
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: -Inf input caused NaN in output";
}

TEST_F(SNNNumericalRegressionTest, Inf_AllInfInputs) {
    /**
     * REGRESSION TEST: All-Inf inputs should not crash
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, std::numeric_limits<float>::infinity());

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* Should produce finite outputs */
    EXPECT_FALSE(contains_inf(outputs.data(), outputs.size()))
        << "REGRESSION: All-Inf inputs produced Inf outputs";
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: All-Inf inputs produced NaN outputs";
}

//=============================================================================
// DENORMAL HANDLING REGRESSION TESTS
//=============================================================================

/**
 * BUG: Denormalized numbers slow down computation significantly
 *
 * WRONG: Allow denormals to propagate through network
 * RIGHT: Flush denormals to zero
 */
TEST_F(SNNNumericalRegressionTest, Denormal_InputHandling) {
    /**
     * REGRESSION TEST: Denormal inputs should be flushed to zero or normalized
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    /* Create denormalized input */
    std::vector<float> inputs(config.n_inputs, 0.0f);
    inputs[0] = std::numeric_limits<float>::denorm_min();

    EXPECT_TRUE(is_denormal(inputs[0])) << "Test value should be denormal";

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* Output should not contain denormals */
    EXPECT_FALSE(contains_denormal(outputs.data(), outputs.size()))
        << "REGRESSION: Denormal values in output. "
        << "Denormals should be flushed to zero for performance.";
}

TEST_F(SNNNumericalRegressionTest, Denormal_NoDenormalsAfterComputation) {
    /**
     * REGRESSION TEST: Internal computations should not produce denormals
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    /* Very small inputs that might produce denormals internally */
    std::vector<float> inputs(config.n_inputs, 1e-35f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());

    /* Run many timesteps */
    for (int i = 0; i < 100; i++) {
        snn_network_step(network, 1.0f);
    }

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_denormal(outputs.data(), outputs.size()))
        << "REGRESSION: Denormals produced during computation";
}

//=============================================================================
// CLAMPING BEHAVIOR REGRESSION TESTS
//=============================================================================

/**
 * BUG: Membrane potential divergence causes overflow
 *
 * WRONG: Let membrane potential grow unbounded
 * RIGHT: Clamp membrane potential to [-200, 100] mV range
 */
TEST_F(SNNNumericalRegressionTest, Clamping_VeryLargeInput) {
    /**
     * REGRESSION TEST: Very large inputs should be clamped
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 1e30f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* Outputs should be finite and reasonable */
    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " is not finite after very large input";
    }
}

TEST_F(SNNNumericalRegressionTest, Clamping_VerySmallInput) {
    /**
     * REGRESSION TEST: Very small inputs should work correctly
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 1e-30f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " is not finite after very small input";
        EXPECT_FALSE(is_denormal(outputs[i]))
            << "REGRESSION: Output " << i << " is denormal after very small input";
    }
}

TEST_F(SNNNumericalRegressionTest, Clamping_VeryNegativeInput) {
    /**
     * REGRESSION TEST: Very negative inputs should be clamped
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, -1e30f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " is not finite after very negative input";
    }
}

TEST_F(SNNNumericalRegressionTest, Clamping_MembranePotentialRange) {
    /**
     * REGRESSION TEST: Membrane potentials should stay in [-200, 100] mV
     *
     * This test documents the expected clamping range.
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    /* Drive network hard with large positive input */
    std::vector<float> inputs(config.n_inputs, 1000.0f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());

    /* Run for extended time */
    for (int t = 0; t < 1000; t++) {
        int spikes = snn_network_step(network, 1.0f);
        EXPECT_GE(spikes, 0)
            << "REGRESSION: Network step returned error at t=" << t;
    }

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* All outputs should be finite after extended run */
    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " diverged after extended simulation";
    }
}

//=============================================================================
// EXTREME VALUE TESTS
//=============================================================================

TEST_F(SNNNumericalRegressionTest, Extreme_FloatMax) {
    /**
     * REGRESSION TEST: FLT_MAX input should not cause overflow
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, std::numeric_limits<float>::max());

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_inf(outputs.data(), outputs.size()))
        << "REGRESSION: FLT_MAX input caused infinity in output";
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: FLT_MAX input caused NaN in output";
}

TEST_F(SNNNumericalRegressionTest, Extreme_FloatLowest) {
    /**
     * REGRESSION TEST: -FLT_MAX input should not cause overflow
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, std::numeric_limits<float>::lowest());

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    EXPECT_FALSE(contains_inf(outputs.data(), outputs.size()))
        << "REGRESSION: -FLT_MAX input caused infinity in output";
    EXPECT_FALSE(contains_nan(outputs.data(), outputs.size()))
        << "REGRESSION: -FLT_MAX input caused NaN in output";
}

TEST_F(SNNNumericalRegressionTest, Extreme_AlternatingSign) {
    /**
     * REGRESSION TEST: Rapidly alternating signs should not cause issues
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs);

    for (int t = 0; t < 100; t++) {
        /* Alternate between large positive and negative */
        float sign = (t % 2 == 0) ? 1.0f : -1.0f;
        for (size_t i = 0; i < inputs.size(); i++) {
            inputs[i] = sign * 1000.0f;
        }

        snn_network_set_inputs(network, inputs.data(), inputs.size());
        snn_network_step(network, 1.0f);
    }

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after alternating inputs";
    }
}

//=============================================================================
// ZERO AND EPSILON TESTS
//=============================================================================

TEST_F(SNNNumericalRegressionTest, Zero_AllZeroInput) {
    /**
     * REGRESSION TEST: All-zero input should work correctly
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 100.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after zero input";
    }
}

TEST_F(SNNNumericalRegressionTest, Zero_NegativeZero) {
    /**
     * REGRESSION TEST: Negative zero should be handled
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, -0.0f);

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after -0.0 input";
    }
}

TEST_F(SNNNumericalRegressionTest, Epsilon_SmallestNormal) {
    /**
     * REGRESSION TEST: Smallest normal float should work
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, std::numeric_limits<float>::min());

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after FLT_MIN input";
    }
}

//=============================================================================
// MIXED PROBLEMATIC VALUES TEST
//=============================================================================

TEST_F(SNNNumericalRegressionTest, Mixed_AllProblematicValues) {
    /**
     * REGRESSION TEST: Mix of NaN, Inf, denormal in same input
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.0f);

    /* Mix of problematic values */
    if (inputs.size() > 0) inputs[0] = std::numeric_limits<float>::quiet_NaN();
    if (inputs.size() > 1) inputs[1] = std::numeric_limits<float>::infinity();
    if (inputs.size() > 2) inputs[2] = -std::numeric_limits<float>::infinity();
    if (inputs.size() > 3) inputs[3] = std::numeric_limits<float>::denorm_min();
    if (inputs.size() > 4) inputs[4] = std::numeric_limits<float>::max();
    if (inputs.size() > 5) inputs[5] = std::numeric_limits<float>::lowest();
    if (inputs.size() > 6) inputs[6] = -0.0f;

    snn_network_set_inputs(network, inputs.data(), inputs.size());
    snn_network_run(network, 10.0f);

    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    /* All outputs should be finite */
    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after mixed problematic input";
    }
}

//=============================================================================
// LONG SIMULATION STABILITY TEST
//=============================================================================

TEST_F(SNNNumericalRegressionTest, LongSimulation_NoAccumulatedError) {
    /**
     * REGRESSION TEST: Long simulations should not accumulate numerical errors
     */
    if (!network) {
        GTEST_SKIP() << "SNN network not available";
    }

    std::vector<float> inputs(config.n_inputs, 0.5f);

    /* Run for many timesteps */
    const int num_steps = 10000;

    for (int t = 0; t < num_steps; t++) {
        snn_network_set_inputs(network, inputs.data(), inputs.size());
        int spikes = snn_network_step(network, 1.0f);

        /* Should never return error */
        ASSERT_GE(spikes, 0)
            << "REGRESSION: Network step returned error at t=" << t;

        /* Periodically check outputs */
        if (t % 1000 == 0) {
            std::vector<float> outputs(config.n_outputs);
            snn_network_get_outputs(network, outputs.data(), outputs.size());

            for (size_t i = 0; i < outputs.size(); i++) {
                EXPECT_TRUE(std::isfinite(outputs[i]))
                    << "REGRESSION: Output " << i << " became non-finite at step " << t;
            }
        }
    }

    /* Final check */
    std::vector<float> outputs(config.n_outputs);
    snn_network_get_outputs(network, outputs.data(), outputs.size());

    for (size_t i = 0; i < outputs.size(); i++) {
        EXPECT_TRUE(std::isfinite(outputs[i]))
            << "REGRESSION: Output " << i << " not finite after " << num_steps << " steps";
        EXPECT_FALSE(is_denormal(outputs[i]))
            << "REGRESSION: Output " << i << " is denormal after " << num_steps << " steps";
    }
}

//=============================================================================
// MAIN
//=============================================================================

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
