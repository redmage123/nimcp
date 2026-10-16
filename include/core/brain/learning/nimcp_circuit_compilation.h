//=============================================================================
// nimcp_circuit_compilation.h - Neural Circuit Compilation Module
//=============================================================================
/**
 * @file nimcp_circuit_compilation.h
 * @brief Compile symbolic rules to neural logic circuits
 *
 * SOLE RESPONSIBILITY: Transform symbolic rules → neural gate networks
 *
 * WHAT: Neural-symbolic bridge - compile logic to circuits
 * WHY:  Enable fast neural evaluation of symbolic rules
 * HOW:  Parse rules, allocate gates, wire connections, verify correctness
 *
 * STRICT SRP:
 * - ONLY compiles rules to circuits (no learning, no reasoning)
 * - ONLY deals with gate-level compilation
 * - Does NOT execute circuits (delegates to inference module)
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_CIRCUIT_COMPILATION_H
#define NIMCP_CIRCUIT_COMPILATION_H

#include "core/brain/nimcp_brain.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Neural logic gate types
 */
typedef enum {
    GATE_AND,                  // Logical AND
    GATE_OR,                   // Logical OR
    GATE_NOT,                  // Logical NOT
    GATE_IMPLIES,              // Logical implication (A→B)
    GATE_INPUT,                // Input terminal
    GATE_OUTPUT                // Output terminal
} gate_type_t;

/**
 * @brief Compiled circuit handle
 */
typedef uint32_t circuit_id_t;

/**
 * @brief Test case for circuit verification
 */
typedef struct {
    const char** inputs;       // Input values (symbolic)
    uint32_t num_inputs;       // Input count
    const char* expected_output; // Expected output (symbolic)
} circuit_test_case_t;

//=============================================================================
// Public API - Circuit Compilation
//=============================================================================

/**
 * @brief Compile symbolic rule to neural logic circuit
 *
 * SOLE PURPOSE: Rule → circuit transformation
 *
 * ALGORITHM:
 * 1. Parse rule string into AST
 * 2. Allocate neural gates for each operator
 * 3. Wire gates according to AST structure
 * 4. Assign circuit ID
 * 5. Store in brain's circuit registry
 *
 * EXAMPLE:
 * - Input: "IF (A AND B) THEN C"
 * - Output: Circuit with AND gate connecting A,B → C
 *
 * @param brain Brain handle
 * @param rule_str Rule in symbolic format
 * @return Circuit ID on success, 0 on error
 */
circuit_id_t compile_rule_to_circuit(brain_t brain, const char* rule_str);

/**
 * @brief Optimize compiled circuit
 *
 * SOLE PURPOSE: Circuit-level optimization
 *
 * OPTIMIZATIONS:
 * - Constant propagation: Eliminate always-true/false gates
 * - Dead code elimination: Remove unreachable gates
 * - Gate fusion: Combine redundant operations
 * - Common subexpression elimination: Share identical subcircuits
 *
 * @param brain Brain handle
 * @param circuit_id Circuit to optimize
 * @return true on success, false on error
 */
bool optimize_circuit(brain_t brain, circuit_id_t circuit_id);

/**
 * @brief Verify circuit correctness against test cases
 *
 * SOLE PURPOSE: Formal verification
 *
 * ALGORITHM:
 * 1. For each test case:
 *    - Set input values
 *    - Evaluate circuit
 *    - Compare output to expected
 * 2. Return pass/fail status
 *
 * @param brain Brain handle
 * @param circuit_id Circuit to verify
 * @param test_cases Test cases
 * @param num_cases Number of test cases
 * @return true if all tests pass, false otherwise
 */
bool verify_circuit_correctness(brain_t brain, circuit_id_t circuit_id,
                                const circuit_test_case_t* test_cases,
                                uint32_t num_cases);

/**
 * @brief Get circuit gate count
 *
 * SOLE PURPOSE: Query circuit complexity
 *
 * @param brain Brain handle
 * @param circuit_id Circuit ID
 * @return Number of gates in circuit, or 0 on error
 */
uint32_t get_circuit_gate_count(brain_t brain, circuit_id_t circuit_id);

/**
 * @brief Delete compiled circuit
 *
 * SOLE PURPOSE: Resource cleanup
 *
 * @param brain Brain handle
 * @param circuit_id Circuit to delete
 * @return true on success, false on error
 */
bool delete_circuit(brain_t brain, circuit_id_t circuit_id);

/**
 * @brief Get circuit evaluation count (performance metric)
 *
 * SOLE PURPOSE: Performance monitoring
 *
 * @param brain Brain handle
 * @param circuit_id Circuit ID
 * @return Number of times circuit has been evaluated
 */
uint64_t get_circuit_eval_count(brain_t brain, circuit_id_t circuit_id);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CIRCUIT_COMPILATION_H */
