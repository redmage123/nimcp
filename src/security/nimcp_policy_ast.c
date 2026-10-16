/**
 * @file nimcp_policy_ast.c
 * @brief NIMCP Policy AST Implementation
 */

#include "security/nimcp_policy_ast.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(policy_ast, MESH_ADAPTER_CATEGORY_SECURITY)


/* ========================================================================
 * Helper Functions
 * ======================================================================== */

static char* safe_strdup(const char* str) {
    if (!str) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "str is NULL");

        return NULL;

    }
    char* dup = strdup(str);
    if (!dup) {
        LOG_ERROR("Failed to duplicate string");
    }
    return dup;
}

static nimcp_ast_node_t* create_node(nimcp_ast_type_t type) {
    nimcp_ast_node_t* node = nimcp_calloc(1, sizeof(nimcp_ast_node_t));
    if (!node) {
        LOG_ERROR("Failed to allocate AST node");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "create_node: node is NULL");
        return NULL;
    }
    node->magic = NIMCP_AST_NODE_MAGIC;
    node->type = type;
    node->location.filename = NULL;
    node->location.line = 0;
    node->location.column = 0;
    return node;
}

/* ========================================================================
 * Node Creation Functions
 * ======================================================================== */

nimcp_ast_node_t* nimcp_ast_create_policy(
    const char* name,
    nimcp_ast_node_t** rules,
    size_t num_rules,
    nimcp_ast_node_t** params,
    size_t num_params)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_POLICY);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->policy.name = safe_strdup(name);
    node->policy.rules = rules;
    node->policy.num_rules = num_rules;
    node->policy.params = params;
    node->policy.num_params = num_params;

    LOG_DEBUG("Created policy AST node: %s", name ? name : "(anonymous)");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_rule(
    const char* name,
    nimcp_ast_node_t* condition,
    nimcp_ast_node_t* action,
    nimcp_ast_node_t** params,
    size_t num_params)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_RULE);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->rule.name = safe_strdup(name);
    node->rule.condition = condition;
    node->rule.action = action;
    node->rule.params = params;
    node->rule.num_params = num_params;

    LOG_DEBUG("Created rule AST node: %s", name ? name : "(anonymous)");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_action(
    const char* action_type,
    nimcp_ast_node_t** params,
    size_t num_params)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_ACTION);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->action.action_type = safe_strdup(action_type);
    node->action.params = params;
    node->action.num_params = num_params;

    LOG_DEBUG("Created action AST node: %s", action_type ? action_type : "UNKNOWN");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_binary(
    nimcp_binary_op_t op,
    nimcp_ast_node_t* left,
    nimcp_ast_node_t* right)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_BINARY_OP);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->binary.op = op;
    node->binary.left = left;
    node->binary.right = right;

    LOG_DEBUG("Created binary op AST node: op=%d", op);
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_unary(
    nimcp_unary_op_t op,
    nimcp_ast_node_t* operand)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_UNARY_OP);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->unary.op = op;
    node->unary.operand = operand;

    LOG_DEBUG("Created unary op AST node: op=%d", op);
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_call(
    const char* name,
    nimcp_ast_node_t** args,
    size_t num_args)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_CALL);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->call.name = safe_strdup(name);
    node->call.args = args;
    node->call.num_args = num_args;

    LOG_DEBUG("Created call AST node: %s(%zu args)", name ? name : "?", num_args);
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_member(
    nimcp_ast_node_t* object,
    const char* member)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_MEMBER);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->member.object = object;
    node->member.member = safe_strdup(member);

    LOG_DEBUG("Created member AST node: .%s", member ? member : "?");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_literal_string(const char* value) {
    nimcp_ast_node_t* node = create_node(NIMCP_AST_LITERAL);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->literal.type = NIMCP_LITERAL_STRING;
    node->literal.string_val = safe_strdup(value);

    LOG_DEBUG("Created string literal AST node: \"%s\"", value ? value : "");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_literal_int(int64_t value) {
    nimcp_ast_node_t* node = create_node(NIMCP_AST_LITERAL);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->literal.type = NIMCP_LITERAL_INT;
    node->literal.int_val = value;

    LOG_DEBUG("Created int literal AST node: %ld", value);
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_literal_float(double value) {
    nimcp_ast_node_t* node = create_node(NIMCP_AST_LITERAL);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->literal.type = NIMCP_LITERAL_FLOAT;
    node->literal.float_val = value;

    LOG_DEBUG("Created float literal AST node: %f", value);
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_literal_bool(bool value) {
    nimcp_ast_node_t* node = create_node(NIMCP_AST_LITERAL);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->literal.type = NIMCP_LITERAL_BOOL;
    node->literal.bool_val = value;

    LOG_DEBUG("Created bool literal AST node: %s", value ? "true" : "false");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_identifier(const char* name) {
    nimcp_ast_node_t* node = create_node(NIMCP_AST_IDENTIFIER);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->identifier.name = safe_strdup(name);

    LOG_DEBUG("Created identifier AST node: %s", name ? name : "?");
    return node;
}

nimcp_ast_node_t* nimcp_ast_create_param(
    const char* key,
    nimcp_ast_node_t* value)
{
    nimcp_ast_node_t* node = create_node(NIMCP_AST_PARAM);
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->param.key = safe_strdup(key);
    node->param.value = value;

    LOG_DEBUG("Created param AST node: %s", key ? key : "?");
    return node;
}

/* ========================================================================
 * Node Manipulation
 * ======================================================================== */

void nimcp_ast_set_location(
    nimcp_ast_node_t* node,
    const char* filename,
    size_t line,
    size_t column)
{
    if (!node || node->magic != NIMCP_AST_NODE_MAGIC) {
        LOG_ERROR("Invalid AST node");
        return;
    }

    node->location.filename = filename;
    node->location.line = line;
    node->location.column = column;
}

void nimcp_ast_destroy(nimcp_ast_node_t* node) {
    if (!node) return;

    if (node->magic != NIMCP_AST_NODE_MAGIC) {
        LOG_ERROR("Invalid AST node magic");
        return;
    }

    switch (node->type) {
        case NIMCP_AST_POLICY:
            nimcp_free(node->policy.name);
            for (size_t i = 0; i < node->policy.num_rules; i++) {
                nimcp_ast_destroy(node->policy.rules[i]);
            }
            nimcp_free(node->policy.rules);
            for (size_t i = 0; i < node->policy.num_params; i++) {
                nimcp_ast_destroy(node->policy.params[i]);
            }
            nimcp_free(node->policy.params);
            break;

        case NIMCP_AST_RULE:
            nimcp_free(node->rule.name);
            nimcp_ast_destroy(node->rule.condition);
            nimcp_ast_destroy(node->rule.action);
            for (size_t i = 0; i < node->rule.num_params; i++) {
                nimcp_ast_destroy(node->rule.params[i]);
            }
            nimcp_free(node->rule.params);
            break;

        case NIMCP_AST_ACTION:
            nimcp_free(node->action.action_type);
            for (size_t i = 0; i < node->action.num_params; i++) {
                nimcp_ast_destroy(node->action.params[i]);
            }
            nimcp_free(node->action.params);
            break;

        case NIMCP_AST_BINARY_OP:
            nimcp_ast_destroy(node->binary.left);
            nimcp_ast_destroy(node->binary.right);
            break;

        case NIMCP_AST_UNARY_OP:
            nimcp_ast_destroy(node->unary.operand);
            break;

        case NIMCP_AST_CALL:
            nimcp_free(node->call.name);
            for (size_t i = 0; i < node->call.num_args; i++) {
                nimcp_ast_destroy(node->call.args[i]);
            }
            nimcp_free(node->call.args);
            break;

        case NIMCP_AST_MEMBER:
            nimcp_ast_destroy(node->member.object);
            nimcp_free(node->member.member);
            break;

        case NIMCP_AST_LITERAL:
            if (node->literal.type == NIMCP_LITERAL_STRING) {
                nimcp_free(node->literal.string_val);
            }
            break;

        case NIMCP_AST_IDENTIFIER:
            nimcp_free(node->identifier.name);
            break;

        case NIMCP_AST_PARAM:
            nimcp_free(node->param.key);
            nimcp_ast_destroy(node->param.value);
            break;

        default:
            LOG_WARN("Unknown AST node type: %d", node->type);
            break;
    }

    node->magic = 0;
    nimcp_free(node);
}

bool nimcp_ast_validate(const nimcp_ast_node_t* node) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ast_validate: node is NULL");
        return false;
    }
    if (node->magic != NIMCP_AST_NODE_MAGIC) {
        return false;
    }

    switch (node->type) {
        case NIMCP_AST_POLICY:
            return node->policy.rules != NULL || node->policy.num_rules == 0;
        case NIMCP_AST_RULE:
            return node->rule.condition != NULL && node->rule.action != NULL;
        case NIMCP_AST_BINARY_OP:
            return node->binary.left != NULL && node->binary.right != NULL;
        case NIMCP_AST_UNARY_OP:
            return node->unary.operand != NULL;
        case NIMCP_AST_CALL:
            return node->call.name != NULL;
        case NIMCP_AST_MEMBER:
            return node->member.object != NULL && node->member.member != NULL;
        case NIMCP_AST_IDENTIFIER:
            return node->identifier.name != NULL;
        default:
            return true;
    }
}

/* ========================================================================
 * Debugging
 * ======================================================================== */

static void print_indent(int indent) {
    for (int i = 0; i < indent; i++) {
        printf("  ");
    }
}

void nimcp_ast_print(const nimcp_ast_node_t* node, int indent) {
    if (!node) {
        print_indent(indent);
        printf("(null)\n");
        return;
    }

    if (node->magic != NIMCP_AST_NODE_MAGIC) {
        print_indent(indent);
        printf("(invalid node)\n");
        return;
    }

    print_indent(indent);

    switch (node->type) {
        case NIMCP_AST_POLICY:
            printf("POLICY: %s\n", node->policy.name ? node->policy.name : "(anonymous)");
            for (size_t i = 0; i < node->policy.num_params; i++) {
                nimcp_ast_print(node->policy.params[i], indent + 1);
            }
            for (size_t i = 0; i < node->policy.num_rules; i++) {
                nimcp_ast_print(node->policy.rules[i], indent + 1);
            }
            break;

        case NIMCP_AST_RULE:
            printf("RULE: %s\n", node->rule.name ? node->rule.name : "(anonymous)");
            print_indent(indent + 1);
            printf("CONDITION:\n");
            nimcp_ast_print(node->rule.condition, indent + 2);
            print_indent(indent + 1);
            printf("ACTION:\n");
            nimcp_ast_print(node->rule.action, indent + 2);
            for (size_t i = 0; i < node->rule.num_params; i++) {
                nimcp_ast_print(node->rule.params[i], indent + 1);
            }
            break;

        case NIMCP_AST_ACTION:
            printf("ACTION: %s\n", node->action.action_type);
            for (size_t i = 0; i < node->action.num_params; i++) {
                nimcp_ast_print(node->action.params[i], indent + 1);
            }
            break;

        case NIMCP_AST_BINARY_OP:
            printf("BINARY_OP: %d\n", node->binary.op);
            nimcp_ast_print(node->binary.left, indent + 1);
            nimcp_ast_print(node->binary.right, indent + 1);
            break;

        case NIMCP_AST_UNARY_OP:
            printf("UNARY_OP: %d\n", node->unary.op);
            nimcp_ast_print(node->unary.operand, indent + 1);
            break;

        case NIMCP_AST_CALL:
            printf("CALL: %s\n", node->call.name);
            for (size_t i = 0; i < node->call.num_args; i++) {
                nimcp_ast_print(node->call.args[i], indent + 1);
            }
            break;

        case NIMCP_AST_MEMBER:
            printf("MEMBER: .%s\n", node->member.member);
            nimcp_ast_print(node->member.object, indent + 1);
            break;

        case NIMCP_AST_LITERAL:
            printf("LITERAL: ");
            switch (node->literal.type) {
                case NIMCP_LITERAL_STRING:
                    printf("\"%s\"\n", node->literal.string_val);
                    break;
                case NIMCP_LITERAL_INT:
                    printf("%ld\n", node->literal.int_val);
                    break;
                case NIMCP_LITERAL_FLOAT:
                    printf("%f\n", node->literal.float_val);
                    break;
                case NIMCP_LITERAL_BOOL:
                    printf("%s\n", node->literal.bool_val ? "true" : "false");
                    break;
            }
            break;

        case NIMCP_AST_IDENTIFIER:
            printf("IDENTIFIER: %s\n", node->identifier.name);
            break;

        case NIMCP_AST_PARAM:
            printf("PARAM: %s =\n", node->param.key);
            nimcp_ast_print(node->param.value, indent + 1);
            break;

        default:
            printf("UNKNOWN\n");
            break;
    }
}

nimcp_ast_node_t* nimcp_ast_clone(const nimcp_ast_node_t* node) {
    if (!node || node->magic != NIMCP_AST_NODE_MAGIC) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_ast_clone: node is NULL");
        return NULL;
    }

    nimcp_ast_node_t* clone = NULL;

    switch (node->type) {
        case NIMCP_AST_POLICY: {
            nimcp_ast_node_t** rules = NULL;
            if (node->policy.num_rules > 0) {
                rules = nimcp_calloc(node->policy.num_rules, sizeof(nimcp_ast_node_t*));
                for (size_t i = 0; i < node->policy.num_rules; i++) {
                    rules[i] = nimcp_ast_clone(node->policy.rules[i]);
                }
            }
            nimcp_ast_node_t** params = NULL;
            if (node->policy.num_params > 0) {
                params = nimcp_calloc(node->policy.num_params, sizeof(nimcp_ast_node_t*));
                for (size_t i = 0; i < node->policy.num_params; i++) {
                    params[i] = nimcp_ast_clone(node->policy.params[i]);
                }
            }
            clone = nimcp_ast_create_policy(
                node->policy.name,
                rules,
                node->policy.num_rules,
                params,
                node->policy.num_params
            );
            break;
        }

        case NIMCP_AST_LITERAL:
            switch (node->literal.type) {
                case NIMCP_LITERAL_STRING:
                    clone = nimcp_ast_create_literal_string(node->literal.string_val);
                    break;
                case NIMCP_LITERAL_INT:
                    clone = nimcp_ast_create_literal_int(node->literal.int_val);
                    break;
                case NIMCP_LITERAL_FLOAT:
                    clone = nimcp_ast_create_literal_float(node->literal.float_val);
                    break;
                case NIMCP_LITERAL_BOOL:
                    clone = nimcp_ast_create_literal_bool(node->literal.bool_val);
                    break;
            }
            break;

        case NIMCP_AST_IDENTIFIER:
            clone = nimcp_ast_create_identifier(node->identifier.name);
            break;

        default:
            LOG_WARN("Clone not fully implemented for node type %d", node->type);
            break;
    }

    if (clone) {
        clone->location = node->location;
    }

    return clone;
}
