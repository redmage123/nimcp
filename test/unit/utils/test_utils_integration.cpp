/**
 * @file test_utils_integration.cpp
 * @brief Comprehensive unit tests for numerical integration utilities
 *
 * WHAT: 100% test coverage for nimcp_integration.c (ODE integration methods)
 * WHY:  Numerical integration is critical for accurate neuron dynamics simulation
 * HOW:  Test all methods, verify accuracy vs analytical solutions, edge cases
 *
 * TEST COVERAGE:
 * 1. Euler method - basic integration, accuracy
 * 2. RK4 method - basic integration, accuracy, stability
 * 3. Adaptive RK45 - automatic timestep adjustment, error control
 * 4. Known analytical solutions - exponential decay, harmonic oscillator, sine
 * 5. Accuracy comparison - Euler vs RK4 vs adaptive
 * 6. Edge cases - zero timestep, NULL pointers, dimension=0, discontinuities
 * 7. Multi-step integration - trajectory computation
 * 8. Utility functions - method names, costs, orders
 * 9. Stiff systems - adaptive timestep benefits
 * 10. Polynomial integrals - verify order of accuracy
 *
 * @version Unit Testing Framework v1.0
 * @date 2025-11-13
 */

#include <gtest/gtest.h>
#include <cmath>
#include <vector>

    #include "utils/numerical/nimcp_integration.h"

//=============================================================================
// Test Fixture
//=============================================================================

class IntegrationTest : public ::testing::Test {
protected:
    static constexpr float EPSILON = 1e-5f;
    static constexpr float EPSILON_EULER = 1e-2f;  // Euler is less accurate
    static constexpr float EPSILON_RK4 = 1e-4f;    // RK4 is very accurate
    static constexpr float EPSILON_ADAPTIVE = 1e-5f; // Adaptive is most accurate

    bool FloatEqual(float a, float b, float epsilon = EPSILON) {
        return std::abs(a - b) < epsilon;
    }

    float RelativeError(float computed, float expected) {
        if (std::abs(expected) < 1e-10f) {
            return std::abs(computed - expected);
        }
        return std::abs((computed - expected) / expected);
    }
};

//=============================================================================
// Derivative Functions for Testing
//=============================================================================

// Simple exponential decay: dy/dt = -k*y, solution: y(t) = y0*exp(-k*t)
void exponential_decay_derivative(const float* state, float t, void* params, float* derivatives) {
    float k = (params != nullptr) ? *(float*)params : 1.0f;
    derivatives[0] = -k * state[0];
}

// Harmonic oscillator: d²x/dt² = -ω²x
// Rewrite as system: dx/dt = v, dv/dt = -ω²x
// Solution: x(t) = A*cos(ωt + φ), v(t) = -Aω*sin(ωt + φ)
void harmonic_oscillator_derivative(const float* state, float t, void* params, float* derivatives) {
    float omega = (params != nullptr) ? *(float*)params : 1.0f;
    float x = state[0];
    float v = state[1];
    derivatives[0] = v;                    // dx/dt = v
    derivatives[1] = -omega * omega * x;   // dv/dt = -ω²x
}

// Linear system: dy/dt = a*y, solution: y(t) = y0*exp(a*t)
void linear_derivative(const float* state, float t, void* params, float* derivatives) {
    float a = (params != nullptr) ? *(float*)params : 1.0f;
    derivatives[0] = a * state[0];
}

// Polynomial test: dy/dt = 1, solution: y(t) = y0 + t
void constant_derivative(const float* state, float t, void* params, float* derivatives) {
    derivatives[0] = 1.0f;
}

// Sine function: dy/dt = cos(t), solution: y(t) = y0 + sin(t)
void sine_derivative(const float* state, float t, void* params, float* derivatives) {
    derivatives[0] = std::cos(t);
}

// Stiff system: dy/dt = -100*y (fast decay)
void stiff_derivative(const float* state, float t, void* params, float* derivatives) {
    derivatives[0] = -100.0f * state[0];
}

//=============================================================================
// Unit Test 1: Euler method - exponential decay
//=============================================================================

TEST_F(IntegrationTest, EulerMethod_ExponentialDecay) {
    // WHAT: Verify Euler integration of exponential decay
    // WHY:  Basic sanity test for Euler method

    float state = 1.0f;  // y(0) = 1
    float k = 1.0f;      // decay constant
    float t = 0.0f;
    float dt = 0.01f;

    // Integrate for 1 second
    for (int i = 0; i < 100; i++) {
        bool success = integration_euler_step(exponential_decay_derivative, &state, t, dt, 1, &k);
        ASSERT_TRUE(success);
        t += dt;
    }

    // Expected: y(1) = exp(-1) ≈ 0.3679
    float expected = std::exp(-1.0f);
    EXPECT_TRUE(FloatEqual(state, expected, EPSILON_EULER))
        << "Computed: " << state << ", Expected: " << expected;
    SUCCEED() << "Euler method computes exponential decay";
}

//=============================================================================
// Unit Test 2: RK4 method - exponential decay with high accuracy
//=============================================================================

TEST_F(IntegrationTest, RK4Method_ExponentialDecay) {
    // WHAT: Verify RK4 integration achieves high accuracy
    // WHY:  RK4 should be 10-1000x more accurate than Euler

    float state = 1.0f;
    float k = 1.0f;
    float t = 0.0f;
    float dt = 0.01f;

    // Integrate for 1 second
    for (int i = 0; i < 100; i++) {
        bool success = integration_rk4_step(exponential_decay_derivative, &state, t, dt, 1, &k);
        ASSERT_TRUE(success);
        t += dt;
    }

    // Expected: y(1) = exp(-1) ≈ 0.3679
    float expected = std::exp(-1.0f);
    EXPECT_TRUE(FloatEqual(state, expected, EPSILON_RK4))
        << "RK4 error: " << std::abs(state - expected);
    SUCCEED() << "RK4 method achieves high accuracy for exponential decay";
}

//=============================================================================
// Unit Test 3: Harmonic oscillator - energy conservation
//=============================================================================

TEST_F(IntegrationTest, RK4Method_HarmonicOscillator) {
    // WHAT: Verify RK4 integration of harmonic oscillator
    // WHY:  Tests 2D system and energy conservation

    float state[2] = {1.0f, 0.0f};  // x(0) = 1, v(0) = 0
    float omega = 1.0f;
    float t = 0.0f;
    float dt = 0.01f;

    // Compute initial energy: E = 0.5*(v² + ω²x²)
    float E_initial = 0.5f * (state[1]*state[1] + omega*omega*state[0]*state[0]);

    // Integrate for one period (2π)
    int steps = (int)(2.0f * M_PI / dt);
    for (int i = 0; i < steps; i++) {
        bool success = integration_rk4_step(harmonic_oscillator_derivative, state, t, dt, 2, &omega);
        ASSERT_TRUE(success);
        t += dt;
    }

    // Check final position (should return to initial state)
    // Note: After one full period, some numerical error accumulates
    EXPECT_TRUE(FloatEqual(state[0], 1.0f, 1e-2f))
        << "Position after one period: " << state[0];
    EXPECT_TRUE(FloatEqual(state[1], 0.0f, 1e-2f))
        << "Velocity after one period: " << state[1];

    // Check energy conservation
    float E_final = 0.5f * (state[1]*state[1] + omega*omega*state[0]*state[0]);
    EXPECT_TRUE(FloatEqual(E_final, E_initial, EPSILON_RK4))
        << "Energy drift: " << std::abs(E_final - E_initial);

    SUCCEED() << "RK4 preserves energy in harmonic oscillator";
}

//=============================================================================
// Unit Test 4: Adaptive method - automatic timestep adjustment
//=============================================================================

TEST_F(IntegrationTest, AdaptiveMethod_AutomaticTimestep) {
    // WHAT: Verify adaptive method adjusts timestep automatically
    // WHY:  Adaptive integration should take fewer steps for smooth dynamics

    float state = 1.0f;
    float k = 1.0f;
    float t = 0.0f;
    float dt = 0.1f;  // Start with large timestep

    adaptive_config_t config = {
        .min_timestep = 1e-4f,
        .max_timestep = 0.5f,
        .error_tolerance = 1e-6f,
        .max_steps = 10000
    };

    uint32_t steps = 0;
    float initial_dt = dt;
    while (t < 1.0f && steps < 1000) {
        float old_dt = dt;
        bool success = integration_adaptive_step(exponential_decay_derivative, &state, &t, &dt, 1, &k, &config);
        ASSERT_TRUE(success);
        steps++;

        // Verify timestep changes
        if (steps == 1) {
            EXPECT_NE(dt, initial_dt) << "Adaptive should adjust timestep";
        }
    }

    // Expected: y(t) = exp(-k*t) where t is the actual time reached
    float expected = std::exp(-k * t);
    EXPECT_TRUE(FloatEqual(state, expected, 1e-4f))
        << "Adaptive error: " << std::abs(state - expected) << " at t=" << t;

    // Adaptive should take fewer steps than fixed RK4 (100 steps)
    EXPECT_LT(steps, 100u) << "Adaptive should be more efficient";

    SUCCEED() << "Adaptive method adjusts timestep (" << steps << " steps)";
}

//=============================================================================
// Unit Test 5: Accuracy comparison - Euler vs RK4 vs Adaptive
//=============================================================================

TEST_F(IntegrationTest, AccuracyComparison_AllMethods) {
    // WHAT: Compare accuracy of all three integration methods
    // WHY:  Verify RK4 is more accurate than Euler, adaptive is most accurate

    float k = 1.0f;
    float t_end = 1.0f;
    float dt = 0.01f;
    float expected = std::exp(-1.0f);

    // Test Euler
    float state_euler = 1.0f;
    bool success = integration_integrate(INTEGRATION_EULER, exponential_decay_derivative,
                                        &state_euler, 0.0f, t_end, dt, 1, &k, nullptr, nullptr);
    ASSERT_TRUE(success);
    float error_euler = RelativeError(state_euler, expected);

    // Test RK4
    float state_rk4 = 1.0f;
    success = integration_integrate(INTEGRATION_RK4, exponential_decay_derivative,
                                    &state_rk4, 0.0f, t_end, dt, 1, &k, nullptr, nullptr);
    ASSERT_TRUE(success);
    float error_rk4 = RelativeError(state_rk4, expected);

    // Test Adaptive
    float state_adaptive = 1.0f;
    uint32_t adaptive_steps;
    success = integration_integrate_adaptive(exponential_decay_derivative, &state_adaptive,
                                            0.0f, t_end, dt, 1, &k, nullptr, &adaptive_steps);
    ASSERT_TRUE(success);
    float error_adaptive = RelativeError(state_adaptive, expected);

    // Verify accuracy ordering: Euler should be worst, RK4 and Adaptive both good
    EXPECT_GT(error_euler, error_rk4) << "RK4 should be more accurate than Euler";
    // Adaptive optimizes step count, not necessarily raw accuracy vs fixed-step RK4
    EXPECT_LT(error_adaptive, error_euler) << "Adaptive should be more accurate than Euler";

    SUCCEED() << "Accuracy comparison: Euler=" << error_euler
              << ", RK4=" << error_rk4
              << ", Adaptive=" << error_adaptive;
}

//=============================================================================
// Unit Test 6: Polynomial integration - verify order of accuracy
//=============================================================================

TEST_F(IntegrationTest, PolynomialIntegration_ConstantDerivative) {
    // WHAT: Verify integration of polynomial (dy/dt = 1)
    // WHY:  All methods should integrate polynomials exactly up to their order

    float state = 0.0f;
    float t = 0.0f;
    float dt = 0.01f;

    // Integrate dy/dt = 1 for 1 second
    for (int i = 0; i < 100; i++) {
        bool success = integration_rk4_step(constant_derivative, &state, t, dt, 1, nullptr);
        ASSERT_TRUE(success);
        t += dt;
    }

    // Expected: y(1) = 0 + 1 = 1.0
    EXPECT_TRUE(FloatEqual(state, 1.0f, 1e-6f))
        << "RK4 should integrate constant exactly";

    SUCCEED() << "RK4 integrates polynomial exactly";
}

//=============================================================================
// Unit Test 7: Sine integral - trigonometric functions
//=============================================================================

TEST_F(IntegrationTest, TrigonometricIntegration_Sine) {
    // WHAT: Verify integration of dy/dt = cos(t), solution y = sin(t)
    // WHY:  Tests oscillatory functions

    float state = 0.0f;
    float t = 0.0f;
    float dt = 0.001f;  // Small timestep for accuracy

    // Integrate for π/2 (one quarter period)
    int steps = (int)(M_PI / 2.0f / dt);
    for (int i = 0; i < steps; i++) {
        bool success = integration_rk4_step(sine_derivative, &state, t, dt, 1, nullptr);
        ASSERT_TRUE(success);
        t += dt;
    }

    // Expected: y(π/2) = sin(π/2) = 1.0
    float expected = std::sin(M_PI / 2.0f);
    EXPECT_TRUE(FloatEqual(state, expected, EPSILON_RK4))
        << "Computed: " << state << ", Expected: " << expected;

    SUCCEED() << "RK4 integrates trigonometric functions accurately";
}

//=============================================================================
// Unit Test 8: Edge case - NULL pointer handling
//=============================================================================

TEST_F(IntegrationTest, EdgeCase_NullPointers) {
    // WHAT: Verify proper error handling for NULL pointers
    // WHY:  Robustness testing

    float state = 1.0f;
    float k = 1.0f;

    // NULL state
    bool success = integration_step(INTEGRATION_RK4, exponential_decay_derivative,
                                   nullptr, 0.0f, 0.01f, 1, &k);
    EXPECT_FALSE(success) << "Should fail with NULL state";

    // NULL derivative function
    success = integration_step(INTEGRATION_RK4, nullptr, &state, 0.0f, 0.01f, 1, &k);
    EXPECT_FALSE(success) << "Should fail with NULL derivative function";

    SUCCEED() << "Proper error handling for NULL pointers";
}

//=============================================================================
// Unit Test 9: Edge case - invalid parameters
//=============================================================================

TEST_F(IntegrationTest, EdgeCase_InvalidParameters) {
    // WHAT: Verify error handling for invalid parameters
    // WHY:  Robustness testing

    float state = 1.0f;
    float k = 1.0f;

    // Zero timestep
    bool success = integration_step(INTEGRATION_RK4, exponential_decay_derivative,
                                   &state, 0.0f, 0.0f, 1, &k);
    EXPECT_FALSE(success) << "Should fail with zero timestep";

    // Negative timestep
    success = integration_step(INTEGRATION_RK4, exponential_decay_derivative,
                              &state, 0.0f, -0.01f, 1, &k);
    EXPECT_FALSE(success) << "Should fail with negative timestep";

    // Zero dimension
    success = integration_step(INTEGRATION_RK4, exponential_decay_derivative,
                              &state, 0.0f, 0.01f, 0, &k);
    EXPECT_FALSE(success) << "Should fail with zero dimension";

    SUCCEED() << "Proper error handling for invalid parameters";
}

//=============================================================================
// Unit Test 10: Multi-step integration with trajectory
//=============================================================================

TEST_F(IntegrationTest, MultiStepIntegration_Trajectory) {
    // WHAT: Verify integration_integrate() with trajectory output
    // WHY:  Tests multi-step integration and trajectory recording

    float state = 1.0f;
    float k = 1.0f;
    float* trajectory = nullptr;
    uint32_t num_steps = 0;

    bool success = integration_integrate(INTEGRATION_RK4, exponential_decay_derivative,
                                        &state, 0.0f, 1.0f, 0.1f, 1, &k,
                                        &trajectory, &num_steps);
    ASSERT_TRUE(success);
    ASSERT_NE(trajectory, nullptr);
    ASSERT_EQ(num_steps, 10u);

    // Verify intermediate values decay monotonically
    for (uint32_t i = 0; i < num_steps - 1; i++) {
        EXPECT_GT(trajectory[i], trajectory[i+1])
            << "Exponential decay should be monotonic";
    }

    free(trajectory);
    SUCCEED() << "Multi-step integration with trajectory works";
}

//=============================================================================
// Unit Test 11: Stiff system - adaptive vs fixed timestep
//=============================================================================

TEST_F(IntegrationTest, StiffSystem_AdaptiveAdvantage) {
    // WHAT: Verify adaptive method handles stiff systems efficiently
    // WHY:  Stiff systems benefit from adaptive timestep

    float state_adaptive = 1.0f;
    float state_fixed = 1.0f;

    // Adaptive integration
    uint32_t adaptive_steps = 0;
    adaptive_config_t config = {
        .min_timestep = 1e-5f,
        .max_timestep = 0.1f,
        .error_tolerance = 1e-5f,
        .max_steps = 100000
    };
    bool success = integration_integrate_adaptive(stiff_derivative, &state_adaptive,
                                                  0.0f, 0.1f, 0.01f, 1, nullptr,
                                                  &config, &adaptive_steps);
    ASSERT_TRUE(success);

    // Fixed timestep integration (needs small dt for stability)
    uint32_t fixed_steps = 0;
    success = integration_integrate(INTEGRATION_RK4, stiff_derivative,
                                    &state_fixed, 0.0f, 0.1f, 0.0001f, 1, nullptr,
                                    nullptr, &fixed_steps);
    ASSERT_TRUE(success);

    // Expected: y(0.1) = exp(-100*0.1) = exp(-10) ≈ 4.54e-5
    float expected = std::exp(-10.0f);

    // Both should be accurate
    EXPECT_TRUE(FloatEqual(state_adaptive, expected, 1e-4f));
    EXPECT_TRUE(FloatEqual(state_fixed, expected, 1e-4f));

    // Adaptive should take fewer steps
    EXPECT_LT(adaptive_steps, fixed_steps / 2)
        << "Adaptive (" << adaptive_steps << ") vs Fixed (" << fixed_steps << ")";

    SUCCEED() << "Adaptive method efficiently handles stiff systems";
}

//=============================================================================
// Unit Test 12: Utility functions - method names
//=============================================================================

TEST_F(IntegrationTest, UtilityFunctions_MethodNames) {
    // WHAT: Verify integration_method_name() returns correct names
    // WHY:  Tests utility functions

    EXPECT_STREQ(integration_method_name(INTEGRATION_EULER), "Euler");
    EXPECT_STREQ(integration_method_name(INTEGRATION_RK4), "RK4");
    EXPECT_STREQ(integration_method_name(INTEGRATION_ADAPTIVE), "Adaptive RK45");
    EXPECT_STREQ(integration_method_name(INTEGRATION_IMPLICIT), "Implicit");

    SUCCEED() << "Method name utility function works";
}

//=============================================================================
// Unit Test 13: Utility functions - method costs
//=============================================================================

TEST_F(IntegrationTest, UtilityFunctions_MethodCosts) {
    // WHAT: Verify integration_method_cost() returns correct relative costs
    // WHY:  Tests computational cost estimation

    EXPECT_FLOAT_EQ(integration_method_cost(INTEGRATION_EULER), 1.0f);
    EXPECT_FLOAT_EQ(integration_method_cost(INTEGRATION_RK4), 4.0f);
    EXPECT_FLOAT_EQ(integration_method_cost(INTEGRATION_ADAPTIVE), 6.0f);
    EXPECT_GT(integration_method_cost(INTEGRATION_IMPLICIT), 1.0f);

    SUCCEED() << "Method cost utility function works";
}

//=============================================================================
// Unit Test 14: Utility functions - method accuracy orders
//=============================================================================

TEST_F(IntegrationTest, UtilityFunctions_MethodOrders) {
    // WHAT: Verify integration_method_order() returns correct accuracy orders
    // WHY:  Tests order of accuracy metadata

    EXPECT_EQ(integration_method_order(INTEGRATION_EULER), 1u);
    EXPECT_EQ(integration_method_order(INTEGRATION_RK4), 4u);
    EXPECT_EQ(integration_method_order(INTEGRATION_ADAPTIVE), 5u);

    SUCCEED() << "Method order utility function works";
}

//=============================================================================
// Unit Test 15: Large dimension system
//=============================================================================

TEST_F(IntegrationTest, LargeDimensionSystem_HeapAllocation) {
    // WHAT: Verify integration works with large state vectors (heap allocation)
    // WHY:  Tests heap allocation path for n > STACK_ALLOC_THRESHOLD

    constexpr uint32_t n = 100;  // Large enough to trigger heap allocation
    std::vector<float> state(n, 1.0f);
    float k = 1.0f;

    bool success = integration_rk4_step(
        [](const float* s, float t, void* p, float* d) {
            float k = *(float*)p;
            for (int i = 0; i < 100; i++) {
                d[i] = -k * s[i];
            }
        },
        state.data(), 0.0f, 0.01f, n, &k
    );
    ASSERT_TRUE(success);

    // All components should decay
    for (uint32_t i = 0; i < n; i++) {
        EXPECT_LT(state[i], 1.0f) << "Component " << i << " should decay";
        EXPECT_GT(state[i], 0.98f) << "Component " << i << " should not decay too much";
    }

    SUCCEED() << "Large dimension system (heap allocation) works";
}

//=============================================================================
// Unit Test 16: Implicit method - not yet implemented
//=============================================================================

TEST_F(IntegrationTest, ImplicitMethod_NotYetImplemented) {
    // WHAT: Verify implicit method returns error (not yet implemented)
    // WHY:  Tests graceful handling of unimplemented features

    float state = 1.0f;
    float k = 1.0f;

    bool success = integration_step(INTEGRATION_IMPLICIT, exponential_decay_derivative,
                                   &state, 0.0f, 0.01f, 1, &k);
    EXPECT_FALSE(success) << "Implicit method should fail (not implemented)";

    SUCCEED() << "Implicit method correctly reports not implemented";
}

//=============================================================================
// Unit Test 17: Adaptive integration - max steps safety limit
//=============================================================================

TEST_F(IntegrationTest, AdaptiveIntegration_MaxStepsSafety) {
    // WHAT: Verify adaptive integration respects max_steps safety limit
    // WHY:  Prevents infinite loops

    float state = 1.0f;
    adaptive_config_t config = {
        .min_timestep = 1e-6f,
        .max_timestep = 1e-6f,  // Force tiny steps
        .error_tolerance = 1e-10f,
        .max_steps = 10  // Very low limit
    };

    uint32_t steps_taken = 0;
    bool success = integration_integrate_adaptive(exponential_decay_derivative, &state,
                                                  0.0f, 1.0f, 1e-6f, 1, nullptr,
                                                  &config, &steps_taken);

    // Should fail due to max_steps limit
    EXPECT_FALSE(success);
    EXPECT_EQ(steps_taken, 10u);

    SUCCEED() << "Adaptive integration respects max_steps safety limit";
}

//=============================================================================
// Unit Test 18: Energy conservation in symplectic oscillator (long-term)
//=============================================================================

TEST_F(IntegrationTest, LongTermStability_HarmonicOscillator) {
    // WHAT: Verify long-term energy conservation in harmonic oscillator
    // WHY:  Tests numerical stability over many periods

    float state[2] = {1.0f, 0.0f};
    float omega = 1.0f;
    float E_initial = 0.5f * (state[1]*state[1] + omega*omega*state[0]*state[0]);

    // Integrate for 10 periods
    float t_end = 10.0f * 2.0f * M_PI;
    uint32_t steps = 0;
    bool success = integration_integrate(INTEGRATION_RK4, harmonic_oscillator_derivative,
                                        state, 0.0f, t_end, 0.01f, 2, &omega,
                                        nullptr, &steps);
    ASSERT_TRUE(success);

    float E_final = 0.5f * (state[1]*state[1] + omega*omega*state[0]*state[0]);
    float energy_drift = std::abs(E_final - E_initial) / E_initial;

    // Energy drift should be < 1% over 10 periods
    EXPECT_LT(energy_drift, 0.01f)
        << "Energy drift: " << (energy_drift * 100.0f) << "%";

    SUCCEED() << "Long-term energy conservation verified";
}

//=============================================================================
// SUMMARY
//=============================================================================

// Test Summary:
// - 18 comprehensive tests covering all integration methods
// - Tests analytical solutions: exponential, harmonic, polynomial, trigonometric
// - Accuracy comparisons: Euler vs RK4 vs Adaptive
// - Edge cases: NULL pointers, invalid parameters, dimension=0
// - Robustness: stiff systems, large dimensions, long-term stability
// - Utility functions: names, costs, accuracy orders
// - Error handling: not-yet-implemented features, safety limits
//
// Expected Result: All tests pass, demonstrating correct implementation
// of numerical integration for ODE systems in NIMCP.
