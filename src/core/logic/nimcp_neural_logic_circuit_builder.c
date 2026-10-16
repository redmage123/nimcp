/**
 * @file nimcp_neural_logic_circuit_builder.c
 * @brief MODULE 3: Neural Logic Circuit Builder Implementation
 * @version 3.0.0
 * @date 2025-11-20
 *
 * WHAT: Expression parser and circuit constructor for neural logic gates
 * WHY:  Single Responsibility: Transform logical expressions into executable circuits
 * HOW:  Recursive descent parser → AST → gate instantiation → circuit wiring
 *
 * @author NIMCP Development Team
 */

#include "core/logic/nimcp_neural_logic_circuit_builder.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "core/logic/nimcp_neural_logic_attachment.h"
#include "core/brain/nimcp_brain_internal.h"
#include "utils/validation/nimcp_validate.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include <string.h>
#include <ctype.h>

#include <stddef.h>  /* for NULL */
// === BIO-ASYNC + LOGGING + UNIFIED MEMORY INTEGRATION ===
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "neural_logic_circuit_builder"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(neural_logic_circuit_builder)

#define BIO_MODULE_ID 0x0137


//=============================================================================
// Constants
//=============================================================================

#define MAX_EXPRESSION_LENGTH 1024

//=============================================================================
// Parser Helper Functions
//=============================================================================

/**
 * @brief Skip whitespace characters
 *
 * WHAT: Advance position past spaces, tabs, newlines
 * WHY:  Allow flexible formatting in expressions
 * HOW:  Loop while isspace() is true
 */
static void skip_whitespace(const char* expr, size_t* pos) {
    if (!expr || !pos) return;

    while (expr[*pos] && isspace((unsigned char)expr[*pos])) {
        (*pos)++;
    }
}

/**
 * @brief Parse variable name (A-Z)
 *
 * WHAT: Extract single uppercase letter as variable
 * WHY:  Variables are terminals in grammar
 * HOW:  Check isupper(), consume character
 */
static bool try_parse_variable(const char* expr, size_t* pos, char* var_name) {
    if (!expr || !pos || !var_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "try_parse_variable: required parameter is NULL (expr, pos, var_name)");
        return false;
    }

    skip_whitespace(expr, pos);

    if (isupper((unsigned char)expr[*pos])) {
        *var_name = expr[*pos];
        (*pos)++;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "try_parse_variable: validation failed");
    return false;
}

/**
 * @brief Parse operator keyword or symbol
 *
 * WHAT: Recognize AND, OR, NOT, XOR, IMPLIES operators
 * WHY:  Convert text tokens to gate types
 * HOW:  String comparison with multiple aliases
 */
static bool try_parse_operator(const char* expr, size_t* pos, logic_gate_type_t* gate_type) {
    if (!expr || !pos || !gate_type) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "try_parse_operator: required parameter is NULL (expr, pos, gate_type)");
        return false;
    }

    skip_whitespace(expr, pos);
    const char* p = expr + *pos;

    // NOT operators (highest precedence)
    if (strncmp(p, "NOT", 3) == 0 && !isalnum((unsigned char)p[3])) {
        *gate_type = LOGIC_GATE_NOT;
        *pos += 3;
        return true;
    }
    if (*p == '!') {
        *gate_type = LOGIC_GATE_NOT;
        (*pos)++;
        return true;
    }

    // AND operators
    if (strncmp(p, "AND", 3) == 0 && !isalnum((unsigned char)p[3])) {
        *gate_type = LOGIC_GATE_AND;
        *pos += 3;
        return true;
    }
    if (*p == '&') {
        *gate_type = LOGIC_GATE_AND;
        (*pos)++;
        return true;
    }

    // OR operators
    if (strncmp(p, "OR", 2) == 0 && !isalnum((unsigned char)p[2])) {
        *gate_type = LOGIC_GATE_OR;
        *pos += 2;
        return true;
    }
    if (*p == '|') {
        *gate_type = LOGIC_GATE_OR;
        (*pos)++;
        return true;
    }

    // XOR operators
    if (strncmp(p, "XOR", 3) == 0 && !isalnum((unsigned char)p[3])) {
        *gate_type = LOGIC_GATE_XOR;
        *pos += 3;
        return true;
    }

    // IMPLIES operators
    if (strncmp(p, "->", 2) == 0) {
        *gate_type = LOGIC_GATE_IMPLIES;
        *pos += 2;
        return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "try_parse_operator: validation failed");
    return false;
}

/**
 * @brief Allocate AST node
 *
 * WHAT: Create new AST node with zero-initialization
 * WHY:  Centralize allocation with error handling
 * HOW:  malloc + memset
 */
static ast_node_t* alloc_ast_node(void) {
    ast_node_t* node = (ast_node_t*)nimcp_malloc(sizeof(ast_node_t));
    if (node) {
        memset(node, 0, sizeof(ast_node_t));
    }
    return node;
}

/**
 * @brief Create variable AST node
 *
 * WHAT: Allocate leaf node for variable
 * WHY:  Encapsulate variable node creation
 * HOW:  Allocate, set type and name
 */
static ast_node_t* create_variable_node(char var_name) {
    ast_node_t* node = alloc_ast_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = AST_NODE_VARIABLE;
    node->data.variable.name = var_name;
    return node;
}

/**
 * @brief Create operator AST node
 *
 * WHAT: Allocate internal node for operator
 * WHY:  Encapsulate operator node creation
 * HOW:  Allocate, set type, gate_type, and children
 */
static ast_node_t* create_operator_node(
    logic_gate_type_t gate_type,
    ast_node_t* left,
    ast_node_t* right
) {
    ast_node_t* node = alloc_ast_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = AST_NODE_OPERATOR;
    node->data.op.gate_type = gate_type;
    node->data.op.left = left;
    node->data.op.right = right;
    return node;
}

/**
 * @brief Recursive descent parser - forward declarations
 */
static ast_node_t* parse_expression(const char* expr, size_t* pos);
static ast_node_t* parse_term(const char* expr, size_t* pos);
static ast_node_t* parse_factor(const char* expr, size_t* pos);
static ast_node_t* parse_primary(const char* expr, size_t* pos);

/**
 * @brief Parse primary expression (variable or parenthesized)
 *
 * WHAT: Parse lowest-level expression unit
 * WHY:  Handle variables and grouped sub-expressions
 * HOW:  Try variable, then try (expression)
 */
static ast_node_t* parse_primary(const char* expr, size_t* pos) {
    skip_whitespace(expr, pos);

    // Try parenthesized expression
    if (expr[*pos] == '(') {
        (*pos)++;
        ast_node_t* node = parse_expression(expr, pos);
        skip_whitespace(expr, pos);
        if (expr[*pos] == ')') {
            (*pos)++;
        } else {
            LOG_ERROR("parse_primary: missing closing parenthesis at position %zu", *pos);
            free_ast(node);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_primary: validation failed");
            return NULL;
        }
        return node;
    }

    // Try variable
    char var_name;
    if (try_parse_variable(expr, pos, &var_name)) {
        return create_variable_node(var_name);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parse_primary: validation failed");
    return NULL;
}

/**
 * @brief Parse factor (NOT operations)
 *
 * WHAT: Handle NOT prefix operator
 * WHY:  NOT has highest precedence
 * HOW:  Try NOT, recurse on factor, else parse primary
 */
static ast_node_t* parse_factor(const char* expr, size_t* pos) {
    logic_gate_type_t gate_type;

    // Try NOT operator
    if (try_parse_operator(expr, pos, &gate_type)) {
        if (gate_type == LOGIC_GATE_NOT) {
            ast_node_t* operand = parse_factor(expr, pos);
            if (!operand) {
                LOG_ERROR("parse_factor: NOT requires operand at position %zu", *pos);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_factor: operand is NULL");
                return NULL;
            }
            return create_operator_node(LOGIC_GATE_NOT, operand, NULL);
        }
    }

    return parse_primary(expr, pos);
}

/**
 * @brief Parse term (AND, XOR operations)
 *
 * WHAT: Handle AND and XOR operators
 * WHY:  AND/XOR have medium precedence
 * HOW:  Left-associative: factor (AND factor)*
 */
static ast_node_t* parse_term(const char* expr, size_t* pos) {
    ast_node_t* left = parse_factor(expr, pos);
    if (!left) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "left is NULL");

        return NULL;

    }

    while (true) {
        skip_whitespace(expr, pos);
        size_t saved_pos = *pos;
        logic_gate_type_t gate_type;

        if (try_parse_operator(expr, pos, &gate_type)) {
            if (gate_type == LOGIC_GATE_AND || gate_type == LOGIC_GATE_XOR) {
                ast_node_t* right = parse_factor(expr, pos);
                if (!right) {
                    LOG_ERROR("parse_term: operator requires right operand at position %zu", *pos);
                    free_ast(left);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_term: right is NULL");
                    return NULL;
                }
                left = create_operator_node(gate_type, left, right);
            } else {
                *pos = saved_pos;  // Backtrack
                break;
            }
        } else {
            break;
        }
    }

    return left;
}

/**
 * @brief Parse expression (OR, IMPLIES operations)
 *
 * WHAT: Handle OR and IMPLIES operators
 * WHY:  OR/IMPLIES have lowest precedence
 * HOW:  Left-associative: term (OR term)*
 */
static ast_node_t* parse_expression(const char* expr, size_t* pos) {
    ast_node_t* left = parse_term(expr, pos);
    if (!left) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "left is NULL");

        return NULL;

    }

    while (true) {
        skip_whitespace(expr, pos);
        size_t saved_pos = *pos;
        logic_gate_type_t gate_type;

        if (try_parse_operator(expr, pos, &gate_type)) {
            if (gate_type == LOGIC_GATE_OR || gate_type == LOGIC_GATE_IMPLIES) {
                ast_node_t* right = parse_term(expr, pos);
                if (!right) {
                    LOG_ERROR("parse_expression: operator requires right operand at position %zu", *pos);
                    free_ast(left);
                    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_expression: right is NULL");
                    return NULL;
                }
                left = create_operator_node(gate_type, left, right);
            } else {
                *pos = saved_pos;  // Backtrack
                break;
            }
        } else {
            break;
        }
    }

    return left;
}

//=============================================================================
// MODULE 3: Circuit Builder API Implementation
//=============================================================================

ast_node_t* parse_logic_expression(const char* expression) {
    // Guard: NULL expression
    if (!nimcp_validate_pointer(expression, "expression")) {
        LOG_ERROR("parse_logic_expression: NULL expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_logic_expression: nimcp_validate_pointer is NULL");
        return NULL;
    }

    // Guard: empty expression
    if (expression[0] == '\0') {
        LOG_ERROR("parse_logic_expression: empty expression");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_logic_expression: validation failed");
        return NULL;
    }

    // Guard: expression too long
    if (strlen(expression) >= MAX_EXPRESSION_LENGTH) {
        LOG_ERROR("parse_logic_expression: expression too long (max %d)", MAX_EXPRESSION_LENGTH);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "parse_logic_expression: capacity exceeded");
        return NULL;
    }

    size_t pos = 0;
    ast_node_t* ast = parse_expression(expression, &pos);

    if (!ast) {
        LOG_ERROR("parse_logic_expression: parse failed for '%s'", expression);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_logic_expression: ast is NULL");
        return NULL;
    }

    // Ensure entire expression was consumed
    skip_whitespace(expression, &pos);
    if (expression[pos] != '\0') {
        LOG_ERROR("parse_logic_expression: unexpected characters at position %zu", pos);
        free_ast(ast);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_logic_expression: validation failed");
        return NULL;
    }

    LOG_DEBUG("parse_logic_expression: successfully parsed '%s'", expression);
    return ast;
}

uint32_t build_circuit_from_ast(
    brain_t brain,
    const ast_node_t* ast
) {
    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("build_circuit_from_ast: NULL brain");
        return UINT32_MAX;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("build_circuit_from_ast: brain has no logic network");
        return UINT32_MAX;
    }

    // Guard: NULL AST
    if (!nimcp_validate_pointer(ast, "ast")) {
        LOG_ERROR("build_circuit_from_ast: NULL AST");
        return UINT32_MAX;
    }

    neural_logic_network_t network = brain_get_neural_logic(brain);

    // Base case: variable leaf node
    if (ast->type == AST_NODE_VARIABLE) {
        // Create variable neuron
        char var_name_str[2] = {ast->data.variable.name, '\0'};
        uint32_t var_id = neural_logic_create_variable(network, var_name_str);
        LOG_DEBUG("build_circuit_from_ast: created variable '%c' as neuron %u",
                  ast->data.variable.name, var_id);
        return var_id;
    }

    // Recursive case: operator node
    if (ast->type == AST_NODE_OPERATOR) {
        logic_gate_type_t gate_type = ast->data.op.gate_type;

        // Build left subtree
        uint32_t left_id = UINT32_MAX;
        if (ast->data.op.left) {
            left_id = build_circuit_from_ast(brain, ast->data.op.left);
            if (left_id == UINT32_MAX) {
                LOG_ERROR("build_circuit_from_ast: failed to build left subtree");
                return UINT32_MAX;
            }
        }

        // Build right subtree (may be NULL for NOT)
        uint32_t right_id = UINT32_MAX;
        if (ast->data.op.right) {
            right_id = build_circuit_from_ast(brain, ast->data.op.right);
            if (right_id == UINT32_MAX) {
                LOG_ERROR("build_circuit_from_ast: failed to build right subtree");
                return UINT32_MAX;
            }
        }

        // Create gate for this operator
        float threshold = 1.5F;  // Default threshold
        if (gate_type == LOGIC_GATE_OR) threshold = 0.5F;
        if (gate_type == LOGIC_GATE_NOT) threshold = 0.5F;
        if (gate_type == LOGIC_GATE_XOR) threshold = 1.0F;
        if (gate_type == LOGIC_GATE_IMPLIES) threshold = 1.0F;

        uint32_t gate_id = neural_logic_create_gate(network, gate_type, threshold);
        if (gate_id == UINT32_MAX) {
            LOG_ERROR("build_circuit_from_ast: failed to create gate");
            return UINT32_MAX;
        }

        // Connect inputs to gate
        if (left_id != UINT32_MAX) {
            float weight = 1.0F;
            if (gate_type == LOGIC_GATE_NOT) weight = -1.0F;  // Inhibitory
            neural_logic_connect(network, left_id, gate_id, weight);
        }

        if (right_id != UINT32_MAX) {
            neural_logic_connect(network, right_id, gate_id, 1.0F);
        }

        LOG_DEBUG("build_circuit_from_ast: created gate %s as neuron %u",
                  neural_logic_gate_name(gate_type), gate_id);

        return gate_id;
    }

    LOG_ERROR("build_circuit_from_ast: unknown AST node type");
    return UINT32_MAX;
}

uint32_t brain_build_logic_circuit(
    brain_t brain,
    const char* expression
) {
    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("brain_build_logic_circuit: NULL brain");
        return UINT32_MAX;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("brain_build_logic_circuit: brain has no logic network");
        return UINT32_MAX;
    }

    // Guard: NULL expression
    if (!nimcp_validate_pointer(expression, "expression")) {
        LOG_ERROR("brain_build_logic_circuit: NULL expression");
        return UINT32_MAX;
    }

    // Parse expression to AST
    ast_node_t* ast = parse_logic_expression(expression);
    if (!ast) {
        LOG_ERROR("brain_build_logic_circuit: failed to parse '%s'", expression);
        return UINT32_MAX;
    }

    // Build circuit from AST
    uint32_t circuit_id = build_circuit_from_ast(brain, ast);

    // Free AST (no longer needed)
    free_ast(ast);

    if (circuit_id == UINT32_MAX) {
        LOG_ERROR("brain_build_logic_circuit: failed to build circuit for '%s'", expression);
        return UINT32_MAX;
    }

    LOG_INFO("brain_build_logic_circuit: built circuit for '%s', root gate %u",
             expression, circuit_id);

    return circuit_id;
}

bool destroy_circuit(
    brain_t brain,
    uint32_t circuit_id
) {
    // Guard: NULL brain
    if (!nimcp_validate_pointer(brain, "brain")) {
        LOG_ERROR("destroy_circuit: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "destroy_circuit: nimcp_validate_pointer is NULL");
        return false;
    }

    // Guard: no logic network
    if (!brain_has_neural_logic(brain)) {
        LOG_ERROR("destroy_circuit: brain has no logic network");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "destroy_circuit: brain_has_neural_logic is NULL");
        return false;
    }

    // NOTE: Placeholder implementation
    // Full implementation would traverse circuit and free gates
    // For now, gates remain in network (acceptable for bounded use)

    LOG_DEBUG("destroy_circuit: circuit %u marked for cleanup (placeholder)", circuit_id);

    return true;
}

void free_ast(ast_node_t* ast) {
    // Guard: NULL ast (NULL-safe)
    if (!ast) {
        return;
    }

    // Recursively free children (for operator nodes)
    if (ast->type == AST_NODE_OPERATOR) {
        free_ast(ast->data.op.left);
        free_ast(ast->data.op.right);
    }

    // Free this node
    nimcp_free(ast);
}
