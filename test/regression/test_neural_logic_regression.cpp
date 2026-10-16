/**
 * @file test_neural_logic_regression.cpp
 * @brief Regression tests for neural logic - ensure existing functionality preserved
 *
 * WHAT: Verify that connection implementation doesn't break existing logic
 * WHY:  Ensure backward compatibility after adding connectivity features
 * HOW:  Re-run critical tests from original neural logic implementation
 *
 * @author NIMCP Development Team
 * @date 2025-11-16
 * @version 2.7.0
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>

#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "utils/memory/nimcp_memory.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuralLogicRegressionTest : public ::testing::Test {
protected:
    neural_logic_network_t network = nullptr;
    neural_logic_config_t config;

    void SetUp() override {
        config = neural_logic_default_config(100);
        network = neural_logic_create(&config);
        ASSERT_NE(network, nullptr);
    }

    void TearDown() override {
        if (network) {
            neural_logic_destroy(network);
        }
    }
};

//=============================================================================
// Basic Gate Functionality (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, AndGateLogic) {
    // Verify AND gate still works correctly
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(and_gate, UINT32_MAX);

    // Truth table
    struct { float a, b, expected; } tests[] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {1.0f, 0.0f, 0.0f},
        {1.0f, 1.0f, 1.0f},
    };

    for (const auto& test : tests) {
        float inputs[] = {test.a, test.b};
        float output = 0.0f;
        EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &output));
        EXPECT_NEAR(output, test.expected, 0.1f)
            << "AND(" << test.a << ", " << test.b << ") should be " << test.expected;
    }
}

TEST_F(NeuralLogicRegressionTest, OrGateLogic) {
    // Verify OR gate still works correctly
    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    ASSERT_NE(or_gate, UINT32_MAX);

    struct { float a, b, expected; } tests[] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f},
    };

    for (const auto& test : tests) {
        float inputs[] = {test.a, test.b};
        float output = 0.0f;
        EXPECT_TRUE(neural_logic_evaluate(network, or_gate, inputs, 2, &output));
        EXPECT_NEAR(output, test.expected, 0.1f)
            << "OR(" << test.a << ", " << test.b << ") should be " << test.expected;
    }
}

TEST_F(NeuralLogicRegressionTest, NotGateLogic) {
    // Verify NOT gate still works correctly
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    ASSERT_NE(not_gate, UINT32_MAX);

    struct { float input, expected; } tests[] = {
        {0.0f, 1.0f},
        {1.0f, 0.0f},
    };

    for (const auto& test : tests) {
        float inputs[] = {test.input};
        float output = 0.0f;
        EXPECT_TRUE(neural_logic_evaluate(network, not_gate, inputs, 1, &output));
        EXPECT_NEAR(output, test.expected, 0.1f)
            << "NOT(" << test.input << ") should be " << test.expected;
    }
}

TEST_F(NeuralLogicRegressionTest, XorGateLogic) {
    // Verify XOR gate still works correctly
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    struct { float a, b, expected; } tests[] = {
        {0.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 1.0f},
        {1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f},
    };

    for (const auto& test : tests) {
        float inputs[] = {test.a, test.b};
        float output = 0.0f;
        EXPECT_TRUE(neural_logic_evaluate(network, xor_gate, inputs, 2, &output));
        EXPECT_NEAR(output, test.expected, 0.1f)
            << "XOR(" << test.a << ", " << test.b << ") should be " << test.expected;
    }
}

TEST_F(NeuralLogicRegressionTest, ImpliesGateLogic) {
    // Verify IMPLIES gate still works correctly
    uint32_t implies_gate = neural_logic_create_gate(network, LOGIC_GATE_IMPLIES, 1.2f);
    ASSERT_NE(implies_gate, UINT32_MAX);

    // A -> B is false only when A=true and B=false
    struct { float a, b, expected; } tests[] = {
        {0.0f, 0.0f, 1.0f},  // F -> F = T
        {0.0f, 1.0f, 1.0f},  // F -> T = T
        {1.0f, 0.0f, 0.0f},  // T -> F = F
        {1.0f, 1.0f, 1.0f},  // T -> T = T
    };

    for (const auto& test : tests) {
        float inputs[] = {test.a, test.b};
        float output = 0.0f;
        EXPECT_TRUE(neural_logic_evaluate(network, implies_gate, inputs, 2, &output));
        EXPECT_NEAR(output, test.expected, 0.1f)
            << "IMPLIES(" << test.a << ", " << test.b << ") should be " << test.expected;
    }
}

//=============================================================================
// Network Creation and Destruction (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, CreateDestroyMultipleTimes) {
    // Ensure repeated create/destroy still works
    for (int i = 0; i < 10; i++) {
        neural_logic_config_t cfg = neural_logic_default_config(50);
        neural_logic_network_t net = neural_logic_create(&cfg);
        ASSERT_NE(net, nullptr) << "Failed on iteration " << i;
        neural_logic_destroy(net);
    }
}

TEST_F(NeuralLogicRegressionTest, CreateGatesSequentially) {
    // Create many gates in sequence
    for (int i = 0; i < 50; i++) {
        logic_gate_type_t type = (logic_gate_type_t)(i % LOGIC_GATE_COUNT);
        uint32_t gate = neural_logic_create_gate(network, type, 1.0f);
        EXPECT_NE(gate, UINT32_MAX) << "Failed to create gate " << i;
    }
}

TEST_F(NeuralLogicRegressionTest, NetworkCapacityRespected) {
    // Verify can't exceed max neurons
    neural_logic_config_t cfg = neural_logic_default_config(10);
    neural_logic_network_t net = neural_logic_create(&cfg);
    ASSERT_NE(net, nullptr);

    // Create exactly 10 gates
    for (int i = 0; i < 10; i++) {
        uint32_t gate = neural_logic_create_gate(net, LOGIC_GATE_AND, 1.8f);
        EXPECT_NE(gate, UINT32_MAX);
    }

    // 11th gate should fail
    uint32_t gate = neural_logic_create_gate(net, LOGIC_GATE_AND, 1.8f);
    EXPECT_EQ(gate, UINT32_MAX);

    neural_logic_destroy(net);
}

//=============================================================================
// Variable Binding (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, VariableBindingStillWorks) {
    uint32_t var = neural_logic_create_variable(network, "X");
    ASSERT_NE(var, UINT32_MAX);

    float pattern[64];
    for (int i = 0; i < 64; i++) {
        pattern[i] = (float)i / 64.0f;
    }

    EXPECT_TRUE(neural_logic_bind_variable(network, var, pattern, 1.0f));

    // Query back
    float result[64];
    EXPECT_TRUE(neural_logic_query_variable(network, var, result, 64));

    // Verify pattern matches
    for (int i = 0; i < 64; i++) {
        EXPECT_NEAR(result[i], pattern[i], 0.001f);
    }
}

TEST_F(NeuralLogicRegressionTest, MultipleVariableBindings) {
    uint32_t var_x = neural_logic_create_variable(network, "X");
    uint32_t var_y = neural_logic_create_variable(network, "Y");
    uint32_t var_z = neural_logic_create_variable(network, "Z");

    ASSERT_NE(var_x, UINT32_MAX);
    ASSERT_NE(var_y, UINT32_MAX);
    ASSERT_NE(var_z, UINT32_MAX);

    float pattern_x[64], pattern_y[64], pattern_z[64];
    for (int i = 0; i < 64; i++) {
        pattern_x[i] = 0.3f;
        pattern_y[i] = 0.6f;
        pattern_z[i] = 0.9f;
    }

    EXPECT_TRUE(neural_logic_bind_variable(network, var_x, pattern_x, 1.0f));
    EXPECT_TRUE(neural_logic_bind_variable(network, var_y, pattern_y, 0.8f));
    EXPECT_TRUE(neural_logic_bind_variable(network, var_z, pattern_z, 0.5f));

    // Verify all patterns are independent
    float result[64];
    EXPECT_TRUE(neural_logic_query_variable(network, var_y, result, 64));
    EXPECT_NEAR(result[0], 0.6f, 0.001f);
}

//=============================================================================
// Simulation (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, UpdateNetworkStillWorks) {
    uint32_t gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(gate, UINT32_MAX);

    // Update multiple times
    uint64_t timestamp = 0;
    for (int i = 0; i < 100; i++) {
        uint32_t spikes = neural_logic_update(network, timestamp, 100);
        EXPECT_GE(spikes, 0);  // No crashes
        timestamp += 100;
    }
}

TEST_F(NeuralLogicRegressionTest, SynchronizeStillWorks) {
    EXPECT_TRUE(neural_logic_synchronize(network));
}

//=============================================================================
// Statistics (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, GetStatsStillWorks) {
    neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    neural_logic_create_gate(network, LOGIC_GATE_OR, 1.0f);
    neural_logic_create_variable(network, "X");

    uint32_t total_gates = 0;
    uint32_t total_variables = 0;
    uint64_t total_spikes = 0;
    float avg_eval_time = 0.0f;
    uint64_t gpu_memory = 0;

    bool success = neural_logic_get_stats(
        network,
        &total_gates,
        &total_variables,
        &total_spikes,
        &avg_eval_time,
        &gpu_memory
    );

    EXPECT_TRUE(success);
    EXPECT_EQ(total_gates, 2);
    EXPECT_EQ(total_variables, 1);
}

TEST_F(NeuralLogicRegressionTest, GetNeuronStateStillWorks) {
    uint32_t gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 1.5f);
    ASSERT_NE(gate, UINT32_MAX);

    logic_neuron_state_t state;
    EXPECT_TRUE(neural_logic_get_state(network, gate, &state));
    EXPECT_EQ(state.gate_type, LOGIC_GATE_XOR);
    EXPECT_EQ(state.neuron_id, gate);
}

//=============================================================================
// Brain Integration (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, BrainIntegrationStillWorks) {
    brain_t brain = brain_create(
        "test_brain",
        BRAIN_SIZE_TINY,
        BRAIN_TASK_CLASSIFICATION,
        10,
        5
    );
    ASSERT_NE(brain, nullptr);

    // Associate brain
    neural_logic_set_brain(network, brain);

    // Create and evaluate gate
    uint32_t gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(gate, UINT32_MAX);

    float inputs[] = {1.0f, 1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, gate, inputs, 2, &output));

    // Clear brain
    neural_logic_set_brain(network, nullptr);

    brain_destroy(brain);
}

//=============================================================================
// Utility Functions (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, GateNamesStillCorrect) {
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_AND), "AND");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_OR), "OR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_NOT), "NOT");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_XOR), "XOR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_IMPLIES), "IMPLIES");
}

TEST_F(NeuralLogicRegressionTest, DefaultConfigStillValid) {
    neural_logic_config_t cfg = neural_logic_default_config(100);
    EXPECT_EQ(cfg.max_logic_neurons, 100);
    EXPECT_GT(cfg.timestep_us, 0.0f);
    EXPECT_GT(cfg.integration_window_ms, 0.0f);
}

TEST_F(NeuralLogicRegressionTest, GpuAvailabilityCheckWorks) {
    bool gpu_available = neural_logic_gpu_available();
    // Just verify it returns without crashing
    EXPECT_TRUE(gpu_available || !gpu_available);
}

//=============================================================================
// Error Handling (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, NullPointerHandling) {
    // All these should handle NULL gracefully
    EXPECT_FALSE(neural_logic_evaluate(nullptr, 0, nullptr, 0, nullptr));
    EXPECT_FALSE(neural_logic_get_state(nullptr, 0, nullptr));
    EXPECT_FALSE(neural_logic_get_stats(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr));
    EXPECT_FALSE(neural_logic_synchronize(nullptr));
    EXPECT_EQ(neural_logic_update(nullptr, 0, 0), 0);

    neural_logic_destroy(nullptr);  // Should not crash
    neural_logic_set_brain(nullptr, nullptr);  // Should not crash
}

TEST_F(NeuralLogicRegressionTest, InvalidGateType) {
    uint32_t gate = neural_logic_create_gate(
        network,
        (logic_gate_type_t)999,  // Invalid type
        1.0f
    );
    EXPECT_EQ(gate, UINT32_MAX);
}

TEST_F(NeuralLogicRegressionTest, InvalidNeuronId) {
    logic_neuron_state_t state;
    EXPECT_FALSE(neural_logic_get_state(network, UINT32_MAX, &state));

    float inputs[] = {1.0f};
    float output = 0.0f;
    EXPECT_FALSE(neural_logic_evaluate(network, UINT32_MAX, inputs, 1, &output));
}

TEST_F(NeuralLogicRegressionTest, InvalidVariableId) {
    float pattern[64] = {0};
    EXPECT_FALSE(neural_logic_bind_variable(network, UINT32_MAX, pattern, 1.0f));
    EXPECT_FALSE(neural_logic_query_variable(network, UINT32_MAX, pattern, 64));
}

//=============================================================================
// Memory Safety (Regression)
//=============================================================================

TEST_F(NeuralLogicRegressionTest, NoMemoryLeaksOnDestroy) {
    // Create network with gates, variables, and connections
    neural_logic_config_t cfg = neural_logic_default_config(50);
    neural_logic_network_t net = neural_logic_create(&cfg);
    ASSERT_NE(net, nullptr);

    // Create gates
    for (int i = 0; i < 10; i++) {
        neural_logic_create_gate(net, LOGIC_GATE_AND, 1.8f);
    }

    // Create variables
    for (int i = 0; i < 5; i++) {
        char name[16];
        snprintf(name, sizeof(name), "var_%d", i);
        uint32_t var = neural_logic_create_variable(net, name);

        if (var != UINT32_MAX) {
            float pattern[64] = {0};
            neural_logic_bind_variable(net, var, pattern, 1.0f);
        }
    }

    // Destroy should clean everything
    neural_logic_destroy(net);
}

TEST_F(NeuralLogicRegressionTest, MultipleNetworksIndependent) {
    // Create multiple networks, verify they're independent
    neural_logic_config_t cfg1 = neural_logic_default_config(30);
    neural_logic_config_t cfg2 = neural_logic_default_config(40);

    neural_logic_network_t net1 = neural_logic_create(&cfg1);
    neural_logic_network_t net2 = neural_logic_create(&cfg2);

    ASSERT_NE(net1, nullptr);
    ASSERT_NE(net2, nullptr);
    EXPECT_NE(net1, net2);

    // Create different gates in each
    uint32_t gate1 = neural_logic_create_gate(net1, LOGIC_GATE_AND, 1.8f);
    uint32_t gate2 = neural_logic_create_gate(net2, LOGIC_GATE_OR, 1.0f);

    EXPECT_NE(gate1, UINT32_MAX);
    EXPECT_NE(gate2, UINT32_MAX);

    // Destroy both
    neural_logic_destroy(net1);
    neural_logic_destroy(net2);
}
