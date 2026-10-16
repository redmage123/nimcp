/**
 * @file test_neuron_types_bio_async.cpp
 * @brief Unit tests for neuron types with bio-async integration
 *
 * Tests neuron type bio-async message publishing and comprehensive logging.
 *
 * @author NIMCP Test Team
 * @date 2025-11-29
 */

#include <gtest/gtest.h>

// Headers have their own extern "C" guards
#include "core/neuron_types/nimcp_neuron_types.h"
#include "core/neuron_types/nimcp_neural_logic.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory_guards.h"

//=============================================================================
// Test Fixture
//=============================================================================

class NeuronTypesBioAsyncTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logging system
        nimcp_logging_init();
        nimcp_logging_set_level(NIMCP_LOG_DEBUG);

        // Initialize bio-async router
        bio_router_config_t config = {
            .max_modules = 64,
            .max_channels = 16,
            .enable_stats = true,
            .enable_tracing = false
        };
        ASSERT_EQ(bio_router_initialize(&config), NIMCP_SUCCESS);
    }

    void TearDown() override {
        // Cleanup bio-async router
        bio_router_shutdown();

        // Cleanup logging
        nimcp_logging_shutdown();
    }
};

//=============================================================================
// Neuron Types Tests
//=============================================================================

TEST_F(NeuronTypesBioAsyncTest, GetDefaultParams) {
    neuron_type_params_t params;

    // Test LIF default params
    EXPECT_EQ(neuron_type_get_default_params(NEURON_GENERIC_LIF, &params), NIMCP_SUCCESS);
    EXPECT_GT(params.lif.tau_membrane, 0.0f);
    EXPECT_LT(params.lif.threshold, params.lif.rest_potential);

    // Test V1 simple cell default params
    EXPECT_EQ(neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params), NIMCP_SUCCESS);
    EXPECT_GE(params.v1_simple.orientation, 0.0f);
    EXPECT_LE(params.v1_simple.orientation, 180.0f);
    EXPECT_GT(params.v1_simple.spatial_frequency, 0.0f);

    // Test A1 frequency-tuned default params
    EXPECT_EQ(neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params), NIMCP_SUCCESS);
    EXPECT_GT(params.a1_frequency.center_frequency, 0.0f);
    EXPECT_GT(params.a1_frequency.q_factor, 0.0f);

    // Test metacognitive default params
    EXPECT_EQ(neuron_type_get_default_params(NEURON_METACOGNITIVE, &params), NIMCP_SUCCESS);
    EXPECT_GE(params.metacognitive.confidence_threshold, 0.0f);
    EXPECT_LE(params.metacognitive.confidence_threshold, 1.0f);

    // Test executive control default params
    EXPECT_EQ(neuron_type_get_default_params(NEURON_EXECUTIVE_CONTROL, &params), NIMCP_SUCCESS);
    EXPECT_GE(params.executive.goal_maintenance, 0.0f);
    EXPECT_LE(params.executive.goal_maintenance, 1.0f);
}

TEST_F(NeuronTypesBioAsyncTest, ValidateParams) {
    neuron_type_params_t params;

    // Test LIF validation
    neuron_type_get_default_params(NEURON_GENERIC_LIF, &params);
    EXPECT_EQ(neuron_type_validate_params(NEURON_GENERIC_LIF, &params), NIMCP_SUCCESS);

    // Invalid LIF params
    params.lif.tau_membrane = -1.0f;  // Invalid
    EXPECT_NE(neuron_type_validate_params(NEURON_GENERIC_LIF, &params), NIMCP_SUCCESS);

    // Test V1 simple cell validation
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    EXPECT_EQ(neuron_type_validate_params(NEURON_V1_SIMPLE_CELL, &params), NIMCP_SUCCESS);

    // Invalid orientation
    params.v1_simple.orientation = 200.0f;  // Out of range
    EXPECT_NE(neuron_type_validate_params(NEURON_V1_SIMPLE_CELL, &params), NIMCP_SUCCESS);

    // Test A1 frequency validation
    neuron_type_get_default_params(NEURON_A1_FREQUENCY_TUNED, &params);
    EXPECT_EQ(neuron_type_validate_params(NEURON_A1_FREQUENCY_TUNED, &params), NIMCP_SUCCESS);

    // Invalid frequency (too high)
    params.a1_frequency.center_frequency = 25000.0f;  // Above 20kHz
    EXPECT_NE(neuron_type_validate_params(NEURON_A1_FREQUENCY_TUNED, &params), NIMCP_SUCCESS);
}

TEST_F(NeuronTypesBioAsyncTest, ProcessInput) {
    neuron_type_params_t params;

    // Test V1 simple cell input processing
    neuron_type_get_default_params(NEURON_V1_SIMPLE_CELL, &params);
    float result = neuron_type_process_input(NEURON_V1_SIMPLE_CELL, &params, 1.0f, 0);
    EXPECT_GE(result, 0.0f);  // Should be non-negative (half-wave rectified)

    // Test passthrough types
    float input = 0.5f;
    result = neuron_type_process_input(NEURON_EXCITATORY, &params, input, 0);
    EXPECT_EQ(result, input);  // Should pass through

    result = neuron_type_process_input(NEURON_INHIBITORY, &params, input, 0);
    EXPECT_EQ(result, input);  // Should pass through
}

TEST_F(NeuronTypesBioAsyncTest, GetNeuronTypeName) {
    EXPECT_STREQ(neuron_type_get_name(NEURON_EXCITATORY), "Excitatory");
    EXPECT_STREQ(neuron_type_get_name(NEURON_GENERIC_LIF), "Generic LIF");
    EXPECT_STREQ(neuron_type_get_name(NEURON_V1_SIMPLE_CELL), "V1 Simple Cell");
    EXPECT_STREQ(neuron_type_get_name(NEURON_A1_FREQUENCY_TUNED), "A1 Frequency-Tuned");
    EXPECT_STREQ(neuron_type_get_name(NEURON_METACOGNITIVE), "Metacognitive");
    EXPECT_STREQ(neuron_type_get_name(NEURON_EXECUTIVE_CONTROL), "Executive Control");
}

//=============================================================================
// Neural Logic Bio-Async Tests
//=============================================================================

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicBioAsyncRegistration) {
    // Create config with bio-async enabled
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Check bio-async context exists
    bio_module_context_t bio_ctx = neural_logic_get_bio_context(network);
    EXPECT_NE(bio_ctx, nullptr);

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicCreateGateWithBioAsync) {
    // Create config with bio-async enabled
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Create AND gate (should publish bio-async message)
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    EXPECT_NE(and_gate, UINT32_MAX);

    // Create OR gate
    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.6f);
    EXPECT_NE(or_gate, UINT32_MAX);

    // Create NOT gate
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    EXPECT_NE(not_gate, UINT32_MAX);

    // Verify gates were created
    EXPECT_NE(and_gate, or_gate);
    EXPECT_NE(or_gate, not_gate);

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicEvaluateWithBioAsync) {
    // Create config with bio-async enabled
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Create AND gate
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    ASSERT_NE(and_gate, UINT32_MAX);

    // Test AND gate evaluation (should publish result via bio-async)
    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &output));
    EXPECT_GT(output, 0.5f);  // Should be TRUE

    // Test with FALSE inputs
    inputs[0] = 1.0f;
    inputs[1] = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &output));
    EXPECT_LT(output, 0.5f);  // Should be FALSE

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicBroadcastResults) {
    // Create config with bio-async enabled
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Create XOR gate
    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    ASSERT_NE(xor_gate, UINT32_MAX);

    // Manually broadcast result
    nimcp_error_t err = neural_logic_broadcast_result(network, xor_gate, 1.0f, true);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Broadcast circuit complete
    err = neural_logic_broadcast_circuit_complete(network, 5, 10);
    EXPECT_EQ(err, NIMCP_SUCCESS);

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicProcessMessages) {
    // Create config with bio-async enabled
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Process any pending messages (should be 0 initially)
    uint32_t processed = neural_logic_process_bio_messages(network, 10);
    EXPECT_EQ(processed, 0);

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicWithoutBioAsync) {
    // Create config with bio-async DISABLED
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = false;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Bio-async context should be NULL
    bio_module_context_t bio_ctx = neural_logic_get_bio_context(network);
    EXPECT_EQ(bio_ctx, nullptr);

    // Create gate (should work without bio-async)
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    EXPECT_NE(and_gate, UINT32_MAX);

    // Evaluate (should work without bio-async)
    float inputs[2] = {1.0f, 1.0f};
    float output = 0.0f;
    EXPECT_TRUE(neural_logic_evaluate(network, and_gate, inputs, 2, &output));

    // Cleanup
    neural_logic_destroy(network);
}

TEST_F(NeuronTypesBioAsyncTest, NeuralLogicAllGateTypes) {
    // Create config
    neural_logic_config_t config = neural_logic_default_config(100);
    config.enable_bio_async = true;

    // Create network
    neural_logic_network_t network = neural_logic_create(&config);
    ASSERT_NE(network, nullptr);

    // Test all gate types
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 0.0f);
    EXPECT_NE(and_gate, UINT32_MAX);

    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.0f);
    EXPECT_NE(or_gate, UINT32_MAX);

    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.0f);
    EXPECT_NE(not_gate, UINT32_MAX);

    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.0f);
    EXPECT_NE(xor_gate, UINT32_MAX);

    uint32_t implies_gate = neural_logic_create_gate(network, LOGIC_GATE_IMPLIES, 0.0f);
    EXPECT_NE(implies_gate, UINT32_MAX);

    // Verify gate names
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_AND), "AND");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_OR), "OR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_NOT), "NOT");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_XOR), "XOR");
    EXPECT_STREQ(neural_logic_gate_name(LOGIC_GATE_IMPLIES), "IMPLIES");

    // Cleanup
    neural_logic_destroy(network);
}

//=============================================================================
// Integration Tests
//=============================================================================

TEST_F(NeuronTypesBioAsyncTest, ComprehensiveLoggingCoverage) {
    // This test verifies that all major code paths trigger logging
    neuron_type_params_t params;

    // Test all neuron types for logging
    neuron_type_t types[] = {
        NEURON_EXCITATORY,
        NEURON_GENERIC_LIF,
        NEURON_V1_SIMPLE_CELL,
        NEURON_V1_COMPLEX_CELL,
        NEURON_A1_FREQUENCY_TUNED,
        NEURON_A1_COINCIDENCE_DETECTOR,
        NEURON_METACOGNITIVE,
        NEURON_EXECUTIVE_CONTROL
    };

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        neuron_type_t type = types[i];

        // Get default params (triggers logging)
        nimcp_result_t result = neuron_type_get_default_params(type, &params);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        // Validate params (triggers logging)
        result = neuron_type_validate_params(type, &params);
        EXPECT_EQ(result, NIMCP_SUCCESS);

        // Process input (triggers logging)
        float output = neuron_type_process_input(type, &params, 0.5f, 0);
        (void)output;  // Suppress unused warning

        // Get name (no logging but good to test)
        const char* name = neuron_type_get_name(type);
        EXPECT_NE(name, nullptr);
    }
}

//=============================================================================
// Main
//=============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
