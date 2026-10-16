/**
 * @file nimcp_equation_manipulation.h
 * @brief Symbolic equation manipulation for parietal lobe
 *
 * Implements symbolic mathematics capabilities:
 * - Expression tree parsing and representation
 * - Algebraic operations (simplify, expand, factor)
 * - Symbolic differentiation (chain rule, product rule)
 * - Expression evaluation
 * - Equation solving
 *
 * BIOLOGICAL BASIS:
 * Mathematical symbol manipulation engages the angular gyrus and
 * intraparietal sulcus, with algebraic reasoning requiring
 * integration with prefrontal working memory.
 *
 * USAGE:
 * ```c
 * equation_engine_t* eq = equation_engine_create();
 *
 * // Parse expression
 * expr_node_t* expr = equation_parse(eq, "x^2 + 2*x + 1");
 *
 * // Differentiate
 * expr_node_t* deriv = equation_differentiate(eq, expr, "x");
 *
 * // Evaluate
 * float result = equation_evaluate(eq, expr, vars);
 *
 * equation_free_expr(expr);
 * equation_engine_destroy(eq);
 * ```
 */

#ifndef NIMCP_EQUATION_MANIPULATION_H
#define NIMCP_EQUATION_MANIPULATION_H

#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

/** Maximum expression depth */
#define EQUATION_MAX_DEPTH              32

/** Maximum variable name length */
#define EQUATION_MAX_VAR_NAME           32

/** Maximum variables in expression */
#define EQUATION_MAX_VARIABLES          16

/** Maximum expression string length */
#define EQUATION_MAX_EXPR_STRING        1024

/** Bio-async module ID for equation manipulation */
#define BIO_MODULE_EQUATION             0x0385

/* ============================================================================
 * TYPES
 * ============================================================================ */

/** Opaque handle for equation engine */
typedef struct equation_engine equation_engine_t;

/**
 * @brief Expression node types
 */
typedef enum {
    EXPR_CONSTANT = 0,      /**< Numerical constant */
    EXPR_VARIABLE,          /**< Variable (x, y, etc.) */

    /* Binary operators */
    EXPR_ADD,               /**< Addition (+) */
    EXPR_SUB,               /**< Subtraction (-) */
    EXPR_MUL,               /**< Multiplication (*) */
    EXPR_DIV,               /**< Division (/) */
    EXPR_POW,               /**< Power (^) */

    /* Unary operators */
    EXPR_NEG,               /**< Negation (-x) */

    /* Functions */
    EXPR_SIN,               /**< Sine */
    EXPR_COS,               /**< Cosine */
    EXPR_TAN,               /**< Tangent */
    EXPR_EXP,               /**< Exponential (e^x) */
    EXPR_LOG,               /**< Natural logarithm */
    EXPR_SQRT,              /**< Square root */
    EXPR_ABS,               /**< Absolute value */

    EXPR_TYPE_COUNT
} expr_node_type_t;

/**
 * @brief Expression tree node
 */
typedef struct expr_node {
    expr_node_type_t type;              /**< Node type */

    union {
        float constant;                  /**< Value if EXPR_CONSTANT */
        char variable[EQUATION_MAX_VAR_NAME]; /**< Name if EXPR_VARIABLE */
    } data;

    struct expr_node* left;              /**< Left child (or operand for unary) */
    struct expr_node* right;             /**< Right child (NULL for unary) */

    uint32_t depth;                      /**< Depth in tree */
    bool simplified;                     /**< Already simplified? */
} expr_node_t;

/**
 * @brief Variable binding for evaluation
 */
typedef struct {
    char name[EQUATION_MAX_VAR_NAME];   /**< Variable name */
    float value;                         /**< Variable value */
} variable_binding_t;

/**
 * @brief Equation (lhs = rhs)
 */
typedef struct {
    expr_node_t* lhs;                   /**< Left-hand side */
    expr_node_t* rhs;                   /**< Right-hand side */
} equation_t;

/**
 * @brief Equation manipulation configuration
 */
typedef struct {
    uint32_t max_simplify_iterations;    /**< Max simplification passes (100) */
    uint32_t max_tree_depth;             /**< Max expression tree depth (32) */
    float numerical_tolerance;           /**< Tolerance for numerical comparison (1e-6) */
    bool enable_trigonometric_identities; /**< Use trig identities (true) */
    bool enable_bio_async;               /**< Enable bio-async messaging (false) */

    /* Modulation */
    float inflammation_sensitivity;      /**< Inflammation effect (0-1) */
    float fatigue_sensitivity;           /**< Fatigue effect (0-1) */
} equation_config_t;

/**
 * @brief Equation manipulation statistics
 */
typedef struct {
    uint64_t expressions_parsed;         /**< Total expressions parsed */
    uint64_t simplifications;            /**< Total simplifications */
    uint64_t differentiations;           /**< Total differentiations */
    uint64_t evaluations;                /**< Total evaluations */
    uint64_t equations_solved;           /**< Total equations solved */
    float avg_tree_depth;                /**< Average expression tree depth */
} equation_stats_t;

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

/**
 * @brief Create equation engine with default configuration
 * @return Handle or NULL on error
 */
equation_engine_t* equation_engine_create(void);

/**
 * @brief Create equation engine with custom configuration
 * @param config Configuration (NULL for defaults)
 * @return Handle or NULL on error
 */
equation_engine_t* equation_engine_create_custom(const equation_config_t* config);

/**
 * @brief Destroy equation engine
 * @param eq Handle (NULL safe)
 */
void equation_engine_destroy(equation_engine_t* eq);

/**
 * @brief Get default configuration
 * @return Default configuration struct
 */
equation_config_t equation_default_config(void);

/**
 * @brief Validate configuration
 * @param config Configuration to validate
 * @return true if valid
 */
bool equation_validate_config(const equation_config_t* config);

/* ============================================================================
 * EXPRESSION CREATION API
 * ============================================================================ */

/**
 * @brief Create constant expression node
 * @param eq Equation engine handle
 * @param value Constant value
 * @return Expression node or NULL on error
 */
expr_node_t* equation_create_constant(
    equation_engine_t* eq,
    float value
);

/**
 * @brief Create variable expression node
 * @param eq Equation engine handle
 * @param name Variable name
 * @return Expression node or NULL on error
 */
expr_node_t* equation_create_variable(
    equation_engine_t* eq,
    const char* name
);

/**
 * @brief Create binary operation node
 * @param eq Equation engine handle
 * @param type Operation type (ADD, SUB, MUL, DIV, POW)
 * @param left Left operand
 * @param right Right operand
 * @return Expression node or NULL on error
 */
expr_node_t* equation_create_binary(
    equation_engine_t* eq,
    expr_node_type_t type,
    expr_node_t* left,
    expr_node_t* right
);

/**
 * @brief Create unary operation node
 * @param eq Equation engine handle
 * @param type Operation type (NEG, SIN, COS, etc.)
 * @param operand Operand
 * @return Expression node or NULL on error
 */
expr_node_t* equation_create_unary(
    equation_engine_t* eq,
    expr_node_type_t type,
    expr_node_t* operand
);

/**
 * @brief Free expression tree
 * @param node Expression root (NULL safe)
 */
void equation_free_expr(expr_node_t* node);

/**
 * @brief Deep copy expression tree
 * @param eq Equation engine handle
 * @param node Expression to copy
 * @return Copied expression or NULL on error
 */
expr_node_t* equation_copy_expr(
    equation_engine_t* eq,
    const expr_node_t* node
);

/* ============================================================================
 * PARSING API
 * ============================================================================ */

/**
 * @brief Parse expression string
 *
 * Supports: +, -, *, /, ^, parentheses, functions (sin, cos, exp, log, sqrt)
 *
 * @param eq Equation engine handle
 * @param expr_string Expression string (e.g., "x^2 + 2*x + 1")
 * @return Expression tree or NULL on parse error
 */
expr_node_t* equation_parse(
    equation_engine_t* eq,
    const char* expr_string
);

/**
 * @brief Convert expression tree to string
 *
 * @param eq Equation engine handle
 * @param node Expression tree
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Pointer to buffer or NULL on error
 */
const char* equation_to_string(
    equation_engine_t* eq,
    const expr_node_t* node,
    char* buffer,
    uint32_t buffer_size
);

/* ============================================================================
 * ALGEBRAIC OPERATIONS API
 * ============================================================================ */

/**
 * @brief Simplify expression
 *
 * Applies algebraic simplification rules:
 * - Constant folding (2+3 → 5)
 * - Identity removal (x*1 → x, x+0 → x)
 * - Power simplification (x^0 → 1, x^1 → x)
 *
 * @param eq Equation engine handle
 * @param node Expression to simplify
 * @return Simplified expression (new tree)
 */
expr_node_t* equation_simplify(
    equation_engine_t* eq,
    const expr_node_t* node
);

/**
 * @brief Expand expression (distribute multiplication)
 *
 * Example: (x+1)*(x+2) → x^2 + 3*x + 2
 *
 * @param eq Equation engine handle
 * @param node Expression to expand
 * @return Expanded expression (new tree)
 */
expr_node_t* equation_expand(
    equation_engine_t* eq,
    const expr_node_t* node
);

/**
 * @brief Factor expression (opposite of expand)
 *
 * Example: x^2 + 2*x + 1 → (x+1)^2
 *
 * @param eq Equation engine handle
 * @param node Expression to factor
 * @return Factored expression (new tree)
 */
expr_node_t* equation_factor(
    equation_engine_t* eq,
    const expr_node_t* node
);

/**
 * @brief Substitute variable with expression
 *
 * @param eq Equation engine handle
 * @param node Expression
 * @param var_name Variable to substitute
 * @param replacement Replacement expression
 * @return New expression with substitution
 */
expr_node_t* equation_substitute(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name,
    const expr_node_t* replacement
);

/* ============================================================================
 * CALCULUS API
 * ============================================================================ */

/**
 * @brief Compute symbolic derivative
 *
 * Applies differentiation rules:
 * - Power rule: d/dx[x^n] = n*x^(n-1)
 * - Sum rule: d/dx[f+g] = f' + g'
 * - Product rule: d/dx[f*g] = f'*g + f*g'
 * - Chain rule: d/dx[f(g)] = f'(g) * g'
 *
 * @param eq Equation engine handle
 * @param node Expression to differentiate
 * @param var_name Variable to differentiate with respect to
 * @return Derivative expression (new tree)
 */
expr_node_t* equation_differentiate(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name
);

/**
 * @brief Compute partial derivative
 *
 * Same as differentiate, but named for clarity in multi-variable context.
 *
 * @param eq Equation engine handle
 * @param node Expression
 * @param var_name Variable
 * @return Partial derivative expression
 */
expr_node_t* equation_partial_derivative(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name
);

/**
 * @brief Compute gradient (all partial derivatives)
 *
 * @param eq Equation engine handle
 * @param node Expression
 * @param var_names Array of variable names
 * @param num_vars Number of variables
 * @param gradient Output array of derivative expressions
 * @return Number of derivatives computed
 */
uint32_t equation_gradient(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char** var_names,
    uint32_t num_vars,
    expr_node_t** gradient
);

/* ============================================================================
 * EVALUATION API
 * ============================================================================ */

/**
 * @brief Evaluate expression with variable bindings
 *
 * @param eq Equation engine handle
 * @param node Expression
 * @param bindings Variable bindings
 * @param num_bindings Number of bindings
 * @return Evaluated result (NaN on error)
 */
float equation_evaluate(
    equation_engine_t* eq,
    const expr_node_t* node,
    const variable_binding_t* bindings,
    uint32_t num_bindings
);

/**
 * @brief Check if expression is constant (no variables)
 *
 * @param node Expression
 * @return true if constant
 */
bool equation_is_constant(const expr_node_t* node);

/**
 * @brief Check if expression contains variable
 *
 * @param node Expression
 * @param var_name Variable name
 * @return true if variable is present
 */
bool equation_contains_variable(
    const expr_node_t* node,
    const char* var_name
);

/**
 * @brief Get all variables in expression
 *
 * @param node Expression
 * @param var_names Output array
 * @param max_vars Maximum variables to return
 * @return Number of variables found
 */
uint32_t equation_get_variables(
    const expr_node_t* node,
    char var_names[][EQUATION_MAX_VAR_NAME],
    uint32_t max_vars
);

/* ============================================================================
 * EQUATION SOLVING API
 * ============================================================================ */

/**
 * @brief Create equation (lhs = rhs)
 *
 * @param lhs Left-hand side
 * @param rhs Right-hand side
 * @return Equation structure
 */
equation_t equation_create_equation(
    expr_node_t* lhs,
    expr_node_t* rhs
);

/**
 * @brief Solve equation for variable (isolate variable)
 *
 * Attempts to isolate variable on one side of equation.
 *
 * @param eq Equation engine handle
 * @param eqn Equation to solve
 * @param var_name Variable to solve for
 * @return Solution expression (variable = result) or NULL if unsolvable
 */
expr_node_t* equation_solve_for(
    equation_engine_t* eq,
    const equation_t* eqn,
    const char* var_name
);

/**
 * @brief Find numerical root using Newton's method
 *
 * @param eq Equation engine handle
 * @param node Expression (finds root where node = 0)
 * @param var_name Variable
 * @param initial_guess Starting point
 * @param tolerance Convergence tolerance
 * @param max_iterations Maximum iterations
 * @return Root value (NaN if not converged)
 */
float equation_find_root(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name,
    float initial_guess,
    float tolerance,
    uint32_t max_iterations
);

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

/**
 * @brief Set inflammation level
 *
 * @param eq Equation engine handle
 * @param level Inflammation level [0,1]
 * @return 0 on success
 */
int equation_set_inflammation(
    equation_engine_t* eq,
    float level
);

/**
 * @brief Set fatigue level
 *
 * @param eq Equation engine handle
 * @param level Fatigue level [0,1]
 * @return 0 on success
 */
int equation_set_fatigue(
    equation_engine_t* eq,
    float level
);

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param eq Equation engine handle
 * @param stats Output statistics
 * @return 0 on success
 */
int equation_get_stats(
    equation_engine_t* eq,
    equation_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param eq Equation engine handle
 */
void equation_reset_stats(equation_engine_t* eq);

/**
 * @brief Get last error message
 * @return Thread-local error message
 */
const char* equation_get_last_error(void);

/* ============================================================================
 * UTILITY MACROS
 * ============================================================================ */

/** Create x + y */
#define EXPR_ADD(eq, x, y) equation_create_binary(eq, EXPR_ADD, x, y)

/** Create x - y */
#define EXPR_SUB(eq, x, y) equation_create_binary(eq, EXPR_SUB, x, y)

/** Create x * y */
#define EXPR_MUL(eq, x, y) equation_create_binary(eq, EXPR_MUL, x, y)

/** Create x / y */
#define EXPR_DIV(eq, x, y) equation_create_binary(eq, EXPR_DIV, x, y)

/** Create x ^ y */
#define EXPR_POW(eq, x, y) equation_create_binary(eq, EXPR_POW, x, y)

/** Create sin(x) */
#define EXPR_SIN(eq, x) equation_create_unary(eq, EXPR_SIN, x)

/** Create cos(x) */
#define EXPR_COS(eq, x) equation_create_unary(eq, EXPR_COS, x)

/** Create exp(x) */
#define EXPR_EXP(eq, x) equation_create_unary(eq, EXPR_EXP, x)

/** Create log(x) */
#define EXPR_LOG(eq, x) equation_create_unary(eq, EXPR_LOG, x)

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EQUATION_MANIPULATION_H */
