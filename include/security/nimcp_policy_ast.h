/**
 * @file nimcp_policy_ast.h
 * @brief NIMCP Security Policy AST (Abstract Syntax Tree) Definitions
 *
 * WHAT: Defines the abstract syntax tree node types and structures for the
 *       NIMCP Security Policy Language (NSPL). Provides the in-memory
 *       representation of parsed policy documents.
 *
 * WHY:  The AST serves as the intermediate representation between the text
 *       policy and executable bytecode, enabling optimization, validation,
 *       and compilation. It decouples parsing from evaluation.
 *
 * HOW:  Uses a tagged union (discriminated union) pattern with node types
 *       and type-specific data. Each node contains location information for
 *       error reporting and maintains parent-child relationships.
 */

#ifndef NIMCP_POLICY_AST_H
#define NIMCP_POLICY_AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic number for AST node validation */
#define NIMCP_AST_NODE_MAGIC 0x4E53504C  /* 'NSPL' */

/**
 * AST node types representing all language constructs
 */
typedef enum {
    NIMCP_AST_POLICY,       /* Top-level policy definition */
    NIMCP_AST_RULE,         /* Security rule */
    NIMCP_AST_CONDITION,    /* Conditional expression */
    NIMCP_AST_ACTION,       /* Action specification */
    NIMCP_AST_BINARY_OP,    /* Binary operation (AND, OR, ==, etc.) */
    NIMCP_AST_UNARY_OP,     /* Unary operation (NOT, -) */
    NIMCP_AST_CALL,         /* Function call */
    NIMCP_AST_MEMBER,       /* Member access (input.length) */
    NIMCP_AST_LITERAL,      /* Literal value */
    NIMCP_AST_IDENTIFIER,   /* Variable/identifier */
    NIMCP_AST_PARAM         /* Parameter (key: value) */
} nimcp_ast_type_t;

/**
 * Binary operators
 */
typedef enum {
    NIMCP_OP_AND,
    NIMCP_OP_OR,
    NIMCP_OP_EQ,
    NIMCP_OP_NE,
    NIMCP_OP_LT,
    NIMCP_OP_LE,
    NIMCP_OP_GT,
    NIMCP_OP_GE,
    NIMCP_OP_ADD,
    NIMCP_OP_SUB,
    NIMCP_OP_MUL,
    NIMCP_OP_DIV,
    NIMCP_OP_MOD
} nimcp_binary_op_t;

/**
 * Unary operators
 */
typedef enum {
    NIMCP_OP_NOT,
    NIMCP_OP_NEG
} nimcp_unary_op_t;

/**
 * Literal value types
 */
typedef enum {
    NIMCP_LITERAL_STRING,
    NIMCP_LITERAL_INT,
    NIMCP_LITERAL_FLOAT,
    NIMCP_LITERAL_BOOL
} nimcp_literal_type_t;

/**
 * Source location for error reporting
 */
typedef struct {
    const char* filename;
    size_t line;
    size_t column;
} nimcp_source_location_t;

/* Forward declaration */
typedef struct nimcp_ast_node nimcp_ast_node_t;

/**
 * Literal value union
 */
typedef struct {
    nimcp_literal_type_t type;
    union {
        char* string_val;
        int64_t int_val;
        double float_val;
        bool bool_val;
    };
} nimcp_ast_literal_t;

/**
 * Binary operation node
 */
typedef struct {
    nimcp_binary_op_t op;
    nimcp_ast_node_t* left;
    nimcp_ast_node_t* right;
} nimcp_ast_binary_t;

/**
 * Unary operation node
 */
typedef struct {
    nimcp_unary_op_t op;
    nimcp_ast_node_t* operand;
} nimcp_ast_unary_t;

/**
 * Function call node
 */
typedef struct {
    char* name;
    nimcp_ast_node_t** args;
    size_t num_args;
} nimcp_ast_call_t;

/**
 * Member access node (e.g., input.length)
 */
typedef struct {
    nimcp_ast_node_t* object;
    char* member;
} nimcp_ast_member_t;

/**
 * Identifier node
 */
typedef struct {
    char* name;
} nimcp_ast_identifier_t;

/**
 * Parameter node (key: value)
 */
typedef struct {
    char* key;
    nimcp_ast_node_t* value;
} nimcp_ast_param_t;

/**
 * Action node
 */
typedef struct {
    char* action_type;  /* ALLOW, DENY, THROTTLE, etc. */
    nimcp_ast_node_t** params;
    size_t num_params;
} nimcp_ast_action_t;

/**
 * Rule node
 */
typedef struct {
    char* name;
    nimcp_ast_node_t* condition;
    nimcp_ast_node_t* action;
    nimcp_ast_node_t** params;
    size_t num_params;
} nimcp_ast_rule_t;

/**
 * Policy node
 */
typedef struct {
    char* name;
    nimcp_ast_node_t** rules;
    size_t num_rules;
    nimcp_ast_node_t** params;
    size_t num_params;
} nimcp_ast_policy_t;

/**
 * Generic AST node
 */
struct nimcp_ast_node {
    uint32_t magic;
    nimcp_ast_type_t type;
    nimcp_source_location_t location;
    union {
        nimcp_ast_policy_t policy;
        nimcp_ast_rule_t rule;
        nimcp_ast_action_t action;
        nimcp_ast_binary_t binary;
        nimcp_ast_unary_t unary;
        nimcp_ast_call_t call;
        nimcp_ast_member_t member;
        nimcp_ast_literal_t literal;
        nimcp_ast_identifier_t identifier;
        nimcp_ast_param_t param;
    };
};

/**
 * Create AST nodes
 */
nimcp_ast_node_t* nimcp_ast_create_policy(
    const char* name,
    nimcp_ast_node_t** rules,
    size_t num_rules,
    nimcp_ast_node_t** params,
    size_t num_params
);

nimcp_ast_node_t* nimcp_ast_create_rule(
    const char* name,
    nimcp_ast_node_t* condition,
    nimcp_ast_node_t* action,
    nimcp_ast_node_t** params,
    size_t num_params
);

nimcp_ast_node_t* nimcp_ast_create_action(
    const char* action_type,
    nimcp_ast_node_t** params,
    size_t num_params
);

nimcp_ast_node_t* nimcp_ast_create_binary(
    nimcp_binary_op_t op,
    nimcp_ast_node_t* left,
    nimcp_ast_node_t* right
);

nimcp_ast_node_t* nimcp_ast_create_unary(
    nimcp_unary_op_t op,
    nimcp_ast_node_t* operand
);

nimcp_ast_node_t* nimcp_ast_create_call(
    const char* name,
    nimcp_ast_node_t** args,
    size_t num_args
);

nimcp_ast_node_t* nimcp_ast_create_member(
    nimcp_ast_node_t* object,
    const char* member
);

nimcp_ast_node_t* nimcp_ast_create_literal_string(const char* value);
nimcp_ast_node_t* nimcp_ast_create_literal_int(int64_t value);
nimcp_ast_node_t* nimcp_ast_create_literal_float(double value);
nimcp_ast_node_t* nimcp_ast_create_literal_bool(bool value);

nimcp_ast_node_t* nimcp_ast_create_identifier(const char* name);

nimcp_ast_node_t* nimcp_ast_create_param(
    const char* key,
    nimcp_ast_node_t* value
);

/**
 * Set source location for a node
 */
void nimcp_ast_set_location(
    nimcp_ast_node_t* node,
    const char* filename,
    size_t line,
    size_t column
);

/**
 * Destroy AST node and all children
 */
void nimcp_ast_destroy(nimcp_ast_node_t* node);

/**
 * Clone AST node
 */
nimcp_ast_node_t* nimcp_ast_clone(const nimcp_ast_node_t* node);

/**
 * Validate AST node
 */
bool nimcp_ast_validate(const nimcp_ast_node_t* node);

/**
 * Print AST for debugging
 */
void nimcp_ast_print(const nimcp_ast_node_t* node, int indent);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POLICY_AST_H */
