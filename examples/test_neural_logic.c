/**
 * @file test_neural_logic.c
 * @brief Test Neural Logic Gates
 *
 * WHAT: Test all spiking logic gate types (AND/OR/NOT/XOR/IMPLIES)
 * WHY:  Verify neural logic system works correctly before brain integration
 * HOW:  Create gates, feed inputs, verify truth tables
 *
 * @author NIMCP Development Team
 * @date 2025-11-08
 * @version 2.7.0 Phase 9.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"

//=============================================================================
// Test Helpers
//=============================================================================

void print_header(const char* test_name)
{
    printf("\n");
    printf("============================================================\n");
    printf(" %s\n", test_name);
    printf("============================================================\n");
}

void print_test_result(const char* gate_name, float a, float b, float expected, float actual)
{
    bool passed = (expected == actual);
    printf("  %s(%0.1f, %0.1f) = %0.1f  [Expected: %0.1f] %s\n",
           gate_name, a, b, actual, expected,
           passed ? "✓ PASS" : "✗ FAIL");
}

//=============================================================================
// Test AND Gate
//=============================================================================

bool test_and_gate(neural_logic_network_t network)
{
    print_header("TEST: AND Gate (Coincidence Detection)");

    // Create AND gate
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    if (and_gate == UINT32_MAX) {
        printf("ERROR: Failed to create AND gate\n");
        return false;
    }

    printf("AND Gate Truth Table:\n");

    // Test truth table
    struct {
        float a, b, expected;
    } tests[] = {
        {0.0f, 0.0f, 0.0f},  // 0 AND 0 = 0
        {0.0f, 1.0f, 0.0f},  // 0 AND 1 = 0
        {1.0f, 0.0f, 0.0f},  // 1 AND 0 = 0
        {1.0f, 1.0f, 1.0f},  // 1 AND 1 = 1
    };

    bool all_passed = true;
    for (int i = 0; i < 4; i++) {
        float inputs[2] = {tests[i].a, tests[i].b};
        float output;

        neural_logic_evaluate(network, and_gate, inputs, 2, &output);

        print_test_result("AND", tests[i].a, tests[i].b, tests[i].expected, output);

        if (output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Test OR Gate
//=============================================================================

bool test_or_gate(neural_logic_network_t network)
{
    print_header("TEST: OR Gate (Low Threshold)");

    uint32_t or_gate = neural_logic_create_gate(network, LOGIC_GATE_OR, 0.6f);
    if (or_gate == UINT32_MAX) {
        printf("ERROR: Failed to create OR gate\n");
        return false;
    }

    printf("OR Gate Truth Table:\n");

    struct {
        float a, b, expected;
    } tests[] = {
        {0.0f, 0.0f, 0.0f},  // 0 OR 0 = 0
        {0.0f, 1.0f, 1.0f},  // 0 OR 1 = 1
        {1.0f, 0.0f, 1.0f},  // 1 OR 0 = 1
        {1.0f, 1.0f, 1.0f},  // 1 OR 1 = 1
    };

    bool all_passed = true;
    for (int i = 0; i < 4; i++) {
        float inputs[2] = {tests[i].a, tests[i].b};
        float output;

        neural_logic_evaluate(network, or_gate, inputs, 2, &output);

        print_test_result("OR", tests[i].a, tests[i].b, tests[i].expected, output);

        if (output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Test NOT Gate
//=============================================================================

bool test_not_gate(neural_logic_network_t network)
{
    print_header("TEST: NOT Gate (Inhibitory Interneuron)");

    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);
    if (not_gate == UINT32_MAX) {
        printf("ERROR: Failed to create NOT gate\n");
        return false;
    }

    printf("NOT Gate Truth Table:\n");

    struct {
        float a, expected;
    } tests[] = {
        {0.0f, 1.0f},  // NOT 0 = 1
        {1.0f, 0.0f},  // NOT 1 = 0
    };

    bool all_passed = true;
    for (int i = 0; i < 2; i++) {
        float inputs[1] = {tests[i].a};
        float output;

        neural_logic_evaluate(network, not_gate, inputs, 1, &output);

        printf("  NOT(%0.1f) = %0.1f  [Expected: %0.1f] %s\n",
               tests[i].a, output, tests[i].expected,
               (output == tests[i].expected) ? "✓ PASS" : "✗ FAIL");

        if (output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Test XOR Gate
//=============================================================================

bool test_xor_gate(neural_logic_network_t network)
{
    print_header("TEST: XOR Gate (Differential Detector)");

    uint32_t xor_gate = neural_logic_create_gate(network, LOGIC_GATE_XOR, 0.5f);
    if (xor_gate == UINT32_MAX) {
        printf("ERROR: Failed to create XOR gate\n");
        return false;
    }

    printf("XOR Gate Truth Table:\n");

    struct {
        float a, b, expected;
    } tests[] = {
        {0.0f, 0.0f, 0.0f},  // 0 XOR 0 = 0
        {0.0f, 1.0f, 1.0f},  // 0 XOR 1 = 1
        {1.0f, 0.0f, 1.0f},  // 1 XOR 0 = 1
        {1.0f, 1.0f, 0.0f},  // 1 XOR 1 = 0
    };

    bool all_passed = true;
    for (int i = 0; i < 4; i++) {
        float inputs[2] = {tests[i].a, tests[i].b};
        float output;

        neural_logic_evaluate(network, xor_gate, inputs, 2, &output);

        print_test_result("XOR", tests[i].a, tests[i].b, tests[i].expected, output);

        if (output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Test IMPLIES Gate
//=============================================================================

bool test_implies_gate(neural_logic_network_t network)
{
    print_header("TEST: IMPLIES Gate (A → B ≡ ¬A ∨ B)");

    uint32_t implies_gate = neural_logic_create_gate(network, LOGIC_GATE_IMPLIES, 0.8f);
    if (implies_gate == UINT32_MAX) {
        printf("ERROR: Failed to create IMPLIES gate\n");
        return false;
    }

    printf("IMPLIES Gate Truth Table:\n");

    struct {
        float a, b, expected;
    } tests[] = {
        {0.0f, 0.0f, 1.0f},  // 0 → 0 = 1 (vacuously true)
        {0.0f, 1.0f, 1.0f},  // 0 → 1 = 1
        {1.0f, 0.0f, 0.0f},  // 1 → 0 = 0 (false conclusion)
        {1.0f, 1.0f, 1.0f},  // 1 → 1 = 1
    };

    bool all_passed = true;
    for (int i = 0; i < 4; i++) {
        float inputs[2] = {tests[i].a, tests[i].b};
        float output;

        neural_logic_evaluate(network, implies_gate, inputs, 2, &output);

        print_test_result("→", tests[i].a, tests[i].b, tests[i].expected, output);

        if (output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Test Composite Circuit (NAND = NOT(AND))
//=============================================================================

bool test_composite_nand(neural_logic_network_t network)
{
    print_header("TEST: NAND Composite Circuit (NOT(AND))");

    // Create AND gate and NOT gate
    uint32_t and_gate = neural_logic_create_gate(network, LOGIC_GATE_AND, 1.8f);
    uint32_t not_gate = neural_logic_create_gate(network, LOGIC_GATE_NOT, 0.5f);

    if (and_gate == UINT32_MAX || not_gate == UINT32_MAX) {
        printf("ERROR: Failed to create composite gates\n");
        return false;
    }

    printf("NAND Composite Truth Table:\n");

    struct {
        float a, b, expected;
    } tests[] = {
        {0.0f, 0.0f, 1.0f},  // NAND(0, 0) = 1
        {0.0f, 1.0f, 1.0f},  // NAND(0, 1) = 1
        {1.0f, 0.0f, 1.0f},  // NAND(1, 0) = 1
        {1.0f, 1.0f, 0.0f},  // NAND(1, 1) = 0
    };

    bool all_passed = true;
    for (int i = 0; i < 4; i++) {
        // First pass through AND gate
        float and_inputs[2] = {tests[i].a, tests[i].b};
        float and_output;
        neural_logic_evaluate(network, and_gate, and_inputs, 2, &and_output);

        // Then pass through NOT gate
        float not_inputs[1] = {and_output};
        float nand_output;
        neural_logic_evaluate(network, not_gate, not_inputs, 1, &nand_output);

        print_test_result("NAND", tests[i].a, tests[i].b, tests[i].expected, nand_output);

        if (nand_output != tests[i].expected) {
            all_passed = false;
        }
    }

    return all_passed;
}

//=============================================================================
// Main Test Runner
//=============================================================================

int main(void)
{
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║         NIMCP Neural Logic Gate Test Suite                ║\n");
    printf("║         Testing Spiking Neural Logic Implementation       ║\n");
    printf("║         Version 2.7.0 Phase 9.0                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    // Create neural logic network
    neural_logic_config_t config = neural_logic_default_config(1000);

    printf("\nConfiguration:\n");
    printf("  Max Logic Neurons: %u\n", config.max_logic_neurons);
    printf("  GPU Enabled: %s\n", config.use_gpu ? "YES" : "NO (CPU fallback)");
    printf("  Integration Window: %.1f ms\n", config.integration_window_ms);

    neural_logic_network_t network = neural_logic_create(&config);
    if (!network) {
        printf("\n✗ FATAL: Failed to create neural logic network\n");
        return 1;
    }

    // Run all tests
    int tests_passed = 0;
    int tests_total = 6;

    if (test_and_gate(network)) tests_passed++;
    if (test_or_gate(network)) tests_passed++;
    if (test_not_gate(network)) tests_passed++;
    if (test_xor_gate(network)) tests_passed++;
    if (test_implies_gate(network)) tests_passed++;
    if (test_composite_nand(network)) tests_passed++;

    // Print summary
    print_header("TEST SUMMARY");
    printf("Tests Passed: %d / %d\n", tests_passed, tests_total);
    printf("Success Rate: %.1f%%\n", (tests_passed * 100.0f) / tests_total);

    // Get statistics
    uint32_t total_gates, total_vars;
    uint64_t total_spikes;
    float avg_eval_time;
    uint64_t gpu_memory;

    neural_logic_get_stats(network, &total_gates, &total_vars, &total_spikes,
                          &avg_eval_time, &gpu_memory);

    printf("\nNetwork Statistics:\n");
    printf("  Total Gates Created: %u\n", total_gates);
    printf("  Total Variables: %u\n", total_vars);
    printf("  Total Spikes: %lu\n", total_spikes);
    printf("  Avg Evaluation Time: %.2f μs\n", avg_eval_time);
    if (gpu_memory > 0) {
        printf("  GPU Memory Used: %lu KB\n", gpu_memory / 1024);
    }

    // Clean up
    neural_logic_destroy(network);

    printf("\n");
    if (tests_passed == tests_total) {
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║                 ✓ ALL TESTS PASSED                         ║\n");
        printf("║     Neural Logic System is Working Correctly!             ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n");
        return 0;
    } else {
        printf("╔════════════════════════════════════════════════════════════╗\n");
        printf("║                 ✗ SOME TESTS FAILED                        ║\n");
        printf("║     Please review errors above                             ║\n");
        printf("╚════════════════════════════════════════════════════════════╝\n");
        return 1;
    }
}
