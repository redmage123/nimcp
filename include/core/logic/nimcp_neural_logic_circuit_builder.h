/**
 * @file nimcp_neural_logic_circuit_builder.h
 * @brief MODULE 3: Neural Logic Circuit Builder - Parse Expressions and Build Circuits
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Expression parser and circuit constructor for neural logic gates
 * WHY:  Single Responsibility: Transform logical expressions into executable circuits
 * HOW:  Recursive descent parser → AST → gate instantiation → circuit wiring
 *
 * SINGLE RESPONSIBILITY PRINCIPLE (SRP):
 * - SOLE RESPONSIBILITY: Parse logic expressions and construct neural circuits
 * - DOES: Parse strings, build AST, create gates, connect circuits
 * - DOES NOT: Evaluate gates (MODULE 2), modulate (MODULE 4), manage attachment (MODULE 1)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_NEURAL_LOGIC_CIRCUIT_BUILDER_H
#define NIMCP_NEURAL_LOGIC_CIRCUIT_BUILDER_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuron_types/nimcp_neural_logic.h"
#include "core/brain/nimcp_brain.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// AST (Abstract Syntax Tree) Structures
//=============================================================================

/**
 * @brief AST node type enumeration
 *
 * WHAT: Discriminator for AST node types
 * WHY:  Enable type-safe traversal of syntax tree
 * HOW:  Tag each node as operator or variable
 */
typedef enum {
    AST_NODE_VARIABLE,    // Leaf node: variable (A, B, C...)
    AST_NODE_OPERATOR     // Internal node: logic operator (AND, OR, NOT...)
} ast_node_type_t;

/**
 * @brief Abstract Syntax Tree node
 *
 * WHAT: Recursive tree structure for parsed logic expressions
 * WHY:  Intermediate representation for circuit construction
 * HOW:  Tagged union with operator/variable data
 */
typedef struct ast_node_struct {
    ast_node_type_t type;              // Node type discriminator

    union {
        // VARIABLE node data
        struct {
            char name;                 // Variable name (A-Z)
        } variable;

        // OPERATOR node data
        struct {
            logic_gate_type_t gate_type;    // Logic operation type
            struct ast_node_struct* left;   // Left operand
            struct ast_node_struct* right;  // Right operand (NULL for NOT)
        } op;
    } data;
} ast_node_t;

//=============================================================================
// MODULE 3: Circuit Builder API
//=============================================================================

/**
 * @brief Parse logic expression into AST
 *
 * WHAT: Convert string expression into abstract syntax tree
 * WHY:  Enable structured circuit construction from text
 * HOW:  Recursive descent parser with operator precedence
 *
 * @param expression Logical expression string (e.g., "(A AND B) OR C")
 * @return AST root node, or NULL on parse error
 *
 * GUARD CLAUSES:
 * - NULL expression → NULL + error log
 * - Empty expression → NULL + error log
 * - Syntax error → NULL + error log with position
 *
 * SUPPORTED SYNTAX:
 * - Operators: AND, OR, NOT, XOR, IMPLIES (and symbolic equivalents)
 * - Variables: Single uppercase letters A-Z
 * - Parentheses: ( ) for grouping
 * - Whitespace: Ignored
 *
 * OPERATOR PRECEDENCE (high to low):
 * 1. NOT (¬)
 * 2. AND (∧)
 * 3. XOR (⊕)
 * 4. OR (∨)
 * 5. IMPLIES (→)
 *
 * PARSE ALGORITHM:
 * - Tokenize: Split into operators, variables, parentheses
 * - Build AST: Recursive descent with precedence climbing
 * - Validate: Check balanced parentheses, valid operators
 *
 * COMPLEXITY: O(n) where n = expression length
 * THREAD SAFETY: Thread-safe (no shared state)
 *
 * MEMORY:
 * - Caller must call free_ast() to prevent memory leak
 *
 * EXAMPLE:
 * ```c
 * ast_node_t* ast = parse_logic_expression("(A AND B) OR C");
 * if (ast) {
 *     // Build circuit from AST
 *     uint32_t circuit_id = build_circuit_from_ast(brain, ast);
 *     free_ast(ast);
 * }
 * ```
 */
NIMCP_EXPORT ast_node_t* parse_logic_expression(const char* expression);

/**
 * @brief Build neural circuit from AST
 *
 * WHAT: Construct executable neural logic circuit from syntax tree
 * WHY:  Transform abstract representation into concrete gates and connections
 * HOW:  Post-order AST traversal → create gates → wire connections → return root
 *
 * @param brain Brain instance with attached logic network
 * @param ast AST root node (from parse_logic_expression)
 * @return Root gate ID, or UINT32_MAX on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → UINT32_MAX + error log
 * - brain->logic == NULL → UINT32_MAX + error log
 * - NULL ast → UINT32_MAX + error log
 *
 * CONSTRUCTION ALGORITHM:
 * 1. Post-order traversal: Process children before parent
 * 2. For VARIABLE nodes: Create input neuron
 * 3. For OPERATOR nodes: Create logic gate, connect child outputs to gate inputs
 * 4. Return root gate ID for evaluation
 *
 * GATE CREATION:
 * - AND gate: threshold = 1.5 (requires both inputs)
 * - OR gate: threshold = 0.5 (requires at least one input)
 * - NOT gate: threshold = 0.5 (inhibitory connection)
 * - XOR gate: threshold = 1.0 (balanced excitation/inhibition)
 * - IMPLIES gate: threshold = 1.0 (antecedent → consequent)
 *
 * COMPLEXITY: O(n) where n = AST nodes
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * ast_node_t* ast = parse_logic_expression("A AND B");
 * uint32_t circuit_id = build_circuit_from_ast(brain, ast);
 * free_ast(ast);
 *
 * // Evaluate circuit
 * float inputs[2] = {1.0f, 1.0f};
 * float output;
 * brain_evaluate_logic_gate(brain, circuit_id, inputs, 2, &output);
 * ```
 */
NIMCP_EXPORT uint32_t build_circuit_from_ast(
    brain_t brain,
    const ast_node_t* ast
);

/**
 * @brief Build circuit directly from expression (convenience function)
 *
 * WHAT: One-step parse + build operation
 * WHY:  Simplify common use case of immediate circuit construction
 * HOW:  Parse expression → build circuit → free AST → return circuit ID
 *
 * @param brain Brain instance with attached logic network
 * @param expression Logical expression string
 * @return Root gate ID, or UINT32_MAX on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → UINT32_MAX + error log
 * - brain->logic == NULL → UINT32_MAX + error log
 * - NULL expression → UINT32_MAX + error log
 *
 * BEHAVIOR:
 * - Calls parse_logic_expression(expression)
 * - Calls build_circuit_from_ast(brain, ast)
 * - Calls free_ast(ast) automatically
 * - Returns circuit ID
 *
 * COMPLEXITY: O(n + m) where n = expr length, m = AST nodes
 * THREAD SAFETY: Not thread-safe
 *
 * EXAMPLE:
 * ```c
 * uint32_t circuit = brain_build_logic_circuit(brain, "(A AND B) OR C");
 * if (circuit != UINT32_MAX) {
 *     float bindings[3] = {1.0f, 0.0f, 1.0f};  // A=T, B=F, C=T
 *     float output;
 *     brain_evaluate_logic_gate(brain, circuit, bindings, 3, &output);
 *     // output = (T AND F) OR T = F OR T = T = 1.0
 * }
 * ```
 */
NIMCP_EXPORT uint32_t brain_build_logic_circuit(
    brain_t brain,
    const char* expression
);

/**
 * @brief Destroy circuit and free associated gates
 *
 * WHAT: Clean up neural circuit by removing gates
 * WHY:  Prevent memory leaks for temporary circuits
 * HOW:  Mark gates for deletion, disconnect synapses
 *
 * @param brain Brain instance with attached logic network
 * @param circuit_id Root gate ID of circuit to destroy
 * @return true on success, false on failure
 *
 * GUARD CLAUSES:
 * - NULL brain → false + error log
 * - brain->logic == NULL → false + error log
 * - Invalid circuit_id → false + error log
 *
 * BEHAVIOR:
 * - NOTE: Currently placeholder implementation
 * - Full implementation would traverse circuit and free gates
 * - For now, gates remain in network (acceptable for bounded use)
 *
 * COMPLEXITY: O(n) where n = gates in circuit
 * THREAD SAFETY: Not thread-safe
 */
NIMCP_EXPORT bool destroy_circuit(
    brain_t brain,
    uint32_t circuit_id
);

/**
 * @brief Free AST memory recursively
 *
 * WHAT: Deallocate entire AST tree
 * WHY:  Prevent memory leaks after circuit construction
 * HOW:  Post-order traversal, free children then node
 *
 * @param ast AST root node (NULL-safe)
 *
 * GUARD CLAUSES:
 * - NULL ast → silent return (NULL-safe)
 *
 * BEHAVIOR:
 * - Recursively frees left and right children
 * - Frees current node
 * - Safe to call multiple times with NULL
 *
 * COMPLEXITY: O(n) where n = AST nodes
 * THREAD SAFETY: Thread-safe (no shared state)
 *
 * EXAMPLE:
 * ```c
 * ast_node_t* ast = parse_logic_expression("A AND B");
 * uint32_t circuit = build_circuit_from_ast(brain, ast);
 * free_ast(ast);  // REQUIRED to prevent memory leak
 * ```
 */
NIMCP_EXPORT void free_ast(ast_node_t* ast);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_NEURAL_LOGIC_CIRCUIT_BUILDER_H
