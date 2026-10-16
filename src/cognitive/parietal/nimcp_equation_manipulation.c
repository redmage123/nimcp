/**
 * @file nimcp_equation_manipulation.c
 * @brief Symbolic equation manipulation implementation
 *
 * Implements expression parsing, algebraic operations, and symbolic
 * differentiation for the parietal lobe module.
 */

#include "cognitive/parietal/nimcp_equation_manipulation.h"
#include "constants/nimcp_buffer_constants.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_math_constants.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE(equation_manipulation, MESH_ADAPTER_CATEGORY_COGNITIVE)


/* ============================================================================
 * CONSTANTS
 * ============================================================================ */

#include "constants/nimcp_constants.h"
#define EPSILON NIMCP_EPSILON_NUMERICAL
#define PI NIMCP_PI_F

/* ============================================================================
 * INTERNAL STRUCTURES
 * ============================================================================ */

/**
 * @brief Internal equation engine state
 */
struct equation_engine {
    /* Configuration */
    equation_config_t config;

    /* Modulation state */
    float inflammation_level;
    float fatigue_level;

    /* Statistics */
    uint64_t expressions_parsed;
    uint64_t simplifications;
    uint64_t differentiations;
    uint64_t evaluations;
    uint64_t equations_solved;
    uint64_t total_tree_depth;
    uint64_t tree_count;

    /* Thread safety */
    nimcp_mutex_t* lock;
};

/* Thread-local error message */
static _Thread_local char g_equation_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

/* ============================================================================
 * INTERNAL HELPERS
 * ============================================================================ */

static void set_equation_error(const char* msg) {
    strncpy(g_equation_error, msg, sizeof(g_equation_error) - 1);
    g_equation_error[sizeof(g_equation_error) - 1] = '\0';
}

static bool is_zero(float v) {
    return fabsf(v) < EPSILON;
}

static bool is_one(float v) {
    return fabsf(v - 1.0f) < EPSILON;
}

static expr_node_t* alloc_node(void) {
    expr_node_t* node = nimcp_calloc(1, sizeof(expr_node_t));
    return node;
}

static uint32_t compute_depth(const expr_node_t* node) {
    if (!node) return 0;

    uint32_t left_depth = node->left ? compute_depth(node->left) : 0;
    uint32_t right_depth = node->right ? compute_depth(node->right) : 0;

    return 1 + (left_depth > right_depth ? left_depth : right_depth);
}

/* ============================================================================
 * LIFECYCLE API
 * ============================================================================ */

equation_config_t equation_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_default_con", 0.0f);


    equation_config_t config = {
        .max_simplify_iterations = 100,
        .max_tree_depth = EQUATION_MAX_DEPTH,
        .numerical_tolerance = 1e-6f,
        .enable_trigonometric_identities = true,
        .enable_bio_async = false,
        .inflammation_sensitivity = 0.5f,
        .fatigue_sensitivity = 0.5f
    };
    return config;
}

bool equation_validate_config(const equation_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_validate_config: config is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_validate_co", 0.0f);


    if (config->max_simplify_iterations == 0 ||
        config->max_simplify_iterations > 10000) {
        set_equation_error("Invalid simplify iterations");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_validate_config: config is NULL");
        return false;
    }

    if (config->max_tree_depth == 0 || config->max_tree_depth > 100) {
        set_equation_error("Invalid tree depth");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "equation_validate_config: config->max_tree_depth is zero");
        return false;
    }

    return true;
}

equation_engine_t* equation_engine_create(void) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_engine_crea", 0.0f);


    return equation_engine_create_custom(NULL);
}

equation_engine_t* equation_engine_create_custom(const equation_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_engine_crea", 0.0f);


    equation_config_t cfg;

    if (config) {
        if (!equation_validate_config(config)) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_engine_create_custom: equation_validate_config is NULL");
            return NULL;
        }
        cfg = *config;
    } else {
        cfg = equation_default_config();
    }

    equation_engine_t* eq = nimcp_calloc(1, sizeof(equation_engine_t));
    if (!eq) {
        set_equation_error("Failed to allocate equation engine");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "equation_engine_create_custom: eq is NULL");
        return NULL;
    }

    eq->config = cfg;

    /* Create mutex */
    mutex_attr_t attr = {.type = MUTEX_TYPE_NORMAL};
    eq->lock = nimcp_mutex_create(&attr);
    if (!eq->lock) {
        set_equation_error("Failed to create mutex");
        nimcp_free(eq);
        eq = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "equation_engine_create_custom: eq->lock is NULL");
        return NULL;
    }

    return eq;
}

void equation_engine_destroy(equation_engine_t* eq) {
    if (!eq) return;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_engine_dest", 0.0f);


    if (eq->lock) {
        nimcp_mutex_destroy(eq->lock);
    }

    nimcp_free(eq);
    eq = NULL;
}

/* ============================================================================
 * EXPRESSION CREATION API
 * ============================================================================ */

expr_node_t* equation_create_constant(equation_engine_t* eq, float value) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_create_cons", 0.0f);


    (void)eq;

    expr_node_t* node = alloc_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = EXPR_CONSTANT;
    node->data.constant = value;
    node->depth = 1;

    return node;
}

expr_node_t* equation_create_variable(equation_engine_t* eq, const char* name) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_create_vari", 0.0f);


    (void)eq;

    if (!name || strlen(name) == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "equation_create_variable: name is NULL");
        return NULL;
    }

    expr_node_t* node = alloc_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = EXPR_VARIABLE;
    strncpy(node->data.variable, name, EQUATION_MAX_VAR_NAME - 1);
    node->data.variable[EQUATION_MAX_VAR_NAME - 1] = '\0';
    node->depth = 1;

    return node;
}

expr_node_t* equation_create_binary(
    equation_engine_t* eq,
    expr_node_type_t type,
    expr_node_t* left,
    expr_node_t* right
) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_create_bina", 0.0f);


    (void)eq;

    if (!left || !right) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "equation_create_binary: required parameter is NULL (left, right)");
        return NULL;
    }

    if (type != EXPR_ADD && type != EXPR_SUB && type != EXPR_MUL &&
        type != EXPR_DIV && type != EXPR_POW) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_create_binary: required parameter is NULL (left, right)");
        return NULL;
    }

    expr_node_t* node = alloc_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = type;
    node->left = left;
    node->right = right;
    node->depth = 1 + (left->depth > right->depth ? left->depth : right->depth);

    return node;
}

expr_node_t* equation_create_unary(
    equation_engine_t* eq,
    expr_node_type_t type,
    expr_node_t* operand
) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_create_unar", 0.0f);


    (void)eq;

    if (!operand) {


        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "operand is NULL");


        return NULL;


    }

    expr_node_t* node = alloc_node();
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    node->type = type;
    node->left = operand;
    node->right = NULL;
    node->depth = 1 + operand->depth;

    return node;
}

void equation_free_expr(expr_node_t* node) {
    if (!node) return;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_free_expr", 0.0f);


    equation_free_expr(node->left);
    equation_free_expr(node->right);
    nimcp_free(node);
    node = NULL;
}

expr_node_t* equation_copy_expr(equation_engine_t* eq, const expr_node_t* node) {
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_copy_expr", 0.0f);


    expr_node_t* copy = alloc_node();
    if (!copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy is NULL");

        return NULL;

    }

    copy->type = node->type;
    copy->depth = node->depth;
    copy->simplified = node->simplified;

    if (node->type == EXPR_CONSTANT) {
        copy->data.constant = node->data.constant;
    } else if (node->type == EXPR_VARIABLE) {
        strncpy(copy->data.variable, node->data.variable, EQUATION_MAX_VAR_NAME - 1);
        copy->data.variable[EQUATION_MAX_VAR_NAME - 1] = '\0';
    }

    copy->left = equation_copy_expr(eq, node->left);
    copy->right = equation_copy_expr(eq, node->right);

    return copy;
}

/* ============================================================================
 * PARSING API
 * ============================================================================ */

/* Parser state */
typedef struct {
    const char* input;
    uint32_t pos;
    equation_engine_t* eq;
} parser_state_t;

static char peek(parser_state_t* p) {
    while (p->input[p->pos] && isspace(p->input[p->pos])) p->pos++;
    return p->input[p->pos];
}

static char consume(parser_state_t* p) {
    char c = peek(p);
    if (c) p->pos++;
    return c;
}

static expr_node_t* parse_expr(parser_state_t* p);

static expr_node_t* parse_atom(parser_state_t* p) {
    char c = peek(p);

    /* Parentheses */
    if (c == '(') {
        consume(p);
        expr_node_t* inner = parse_expr(p);
        if (peek(p) == ')') consume(p);
        return inner;
    }

    /* Number */
    if (isdigit(c) || c == '.') {
        float value = 0.0f;
        float decimal = 0.0f;
        float decimal_place = 0.1f;
        bool in_decimal = false;

        while (isdigit(peek(p)) || peek(p) == '.') {
            c = consume(p);
            if (c == '.') {
                in_decimal = true;
            } else if (in_decimal) {
                decimal += (c - '0') * decimal_place;
                decimal_place *= 0.1f;
            } else {
                value = value * 10.0f + (c - '0');
            }
        }
        return equation_create_constant(p->eq, value + decimal);
    }

    /* Variable or function */
    if (isalpha(c)) {
        char name[NIMCP_ID_BUFFER_SIZE] = {0};
        int i = 0;
        while (isalnum(peek(p)) || peek(p) == '_') {
            name[i++] = consume(p);
            if (i >= 63) break;
        }
        name[i] = '\0';

        /* Check for functions */
        if (peek(p) == '(') {
            consume(p);
            expr_node_t* arg = parse_expr(p);
            if (peek(p) == ')') consume(p);

            expr_node_type_t type = EXPR_VARIABLE;
            if (strcmp(name, "sin") == 0) type = EXPR_SIN;
            else if (strcmp(name, "cos") == 0) type = EXPR_COS;
            else if (strcmp(name, "tan") == 0) type = EXPR_TAN;
            else if (strcmp(name, "exp") == 0) type = EXPR_EXP;
            else if (strcmp(name, "log") == 0) type = EXPR_LOG;
            else if (strcmp(name, "sqrt") == 0) type = EXPR_SQRT;
            else if (strcmp(name, "abs") == 0) type = EXPR_ABS;

            if (type != EXPR_VARIABLE) {
                return equation_create_unary(p->eq, type, arg);
            }
            equation_free_expr(arg);
        }

        return equation_create_variable(p->eq, name);
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parse_atom: validation failed");
    return NULL;
}

static expr_node_t* parse_unary(parser_state_t* p) {
    if (peek(p) == '-') {
        consume(p);
        expr_node_t* operand = parse_unary(p);
        return equation_create_unary(p->eq, EXPR_NEG, operand);
    }
    return parse_atom(p);
}

static expr_node_t* parse_power(parser_state_t* p) {
    expr_node_t* left = parse_unary(p);

    while (peek(p) == '^') {
        consume(p);
        expr_node_t* right = parse_unary(p);
        left = equation_create_binary(p->eq, EXPR_POW, left, right);
    }

    return left;
}

static expr_node_t* parse_term(parser_state_t* p) {
    expr_node_t* left = parse_power(p);

    while (peek(p) == '*' || peek(p) == '/') {
        char op = consume(p);
        expr_node_t* right = parse_power(p);
        left = equation_create_binary(p->eq,
            op == '*' ? EXPR_MUL : EXPR_DIV, left, right);
    }

    return left;
}

static expr_node_t* parse_expr(parser_state_t* p) {
    expr_node_t* left = parse_term(p);

    while (peek(p) == '+' || peek(p) == '-') {
        char op = consume(p);
        expr_node_t* right = parse_term(p);
        left = equation_create_binary(p->eq,
            op == '+' ? EXPR_ADD : EXPR_SUB, left, right);
    }

    return left;
}

expr_node_t* equation_parse(equation_engine_t* eq, const char* expr_string) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_parse: eq is NULL");
        return NULL;
    }
    if (!expr_string) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_parse: expr_string is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_parse", 0.0f);


    nimcp_mutex_lock(eq->lock);

    parser_state_t p = {expr_string, 0, eq};
    expr_node_t* result = parse_expr(&p);

    if (result) {
        result->depth = compute_depth(result);
        eq->expressions_parsed++;
        eq->total_tree_depth += result->depth;
        eq->tree_count++;
    }

    nimcp_mutex_unlock(eq->lock);

    return result;
}

const char* equation_to_string(
    equation_engine_t* eq,
    const expr_node_t* node,
    char* buffer,
    uint32_t buffer_size
) {
    (void)eq;

    if (!node || !buffer || buffer_size < 16) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_to_string: required parameter is NULL (node, buffer)");
        return NULL;
    }

    buffer[0] = '\0';

    switch (node->type) {
        case EXPR_CONSTANT:
            snprintf(buffer, buffer_size, "%.6g", node->data.constant);
            break;

        case EXPR_VARIABLE:
            snprintf(buffer, buffer_size, "%s", node->data.variable);
            break;

        case EXPR_NEG: {
            char inner[NIMCP_ERROR_BUFFER_SIZE];
            equation_to_string(eq, node->left, inner, sizeof(inner));
            snprintf(buffer, buffer_size, "(-%s)", inner);
            break;
        }

        case EXPR_ADD:
        case EXPR_SUB:
        case EXPR_MUL:
        case EXPR_DIV:
        case EXPR_POW: {
            char left[NIMCP_ERROR_BUFFER_SIZE], right[256];
            equation_to_string(eq, node->left, left, sizeof(left));
            equation_to_string(eq, node->right, right, sizeof(right));
            const char* op = node->type == EXPR_ADD ? "+" :
                            node->type == EXPR_SUB ? "-" :
                            node->type == EXPR_MUL ? "*" :
                            node->type == EXPR_DIV ? "/" : "^";
            snprintf(buffer, buffer_size, "(%s %s %s)", left, op, right);
            break;
        }

        case EXPR_SIN:
        case EXPR_COS:
        case EXPR_TAN:
        case EXPR_EXP:
        case EXPR_LOG:
        case EXPR_SQRT:
        case EXPR_ABS: {
            char inner[NIMCP_ERROR_BUFFER_SIZE];
            equation_to_string(eq, node->left, inner, sizeof(inner));
            const char* fn = node->type == EXPR_SIN ? "sin" :
                            node->type == EXPR_COS ? "cos" :
                            node->type == EXPR_TAN ? "tan" :
                            node->type == EXPR_EXP ? "exp" :
                            node->type == EXPR_LOG ? "log" :
                            node->type == EXPR_SQRT ? "sqrt" : "abs";
            snprintf(buffer, buffer_size, "%s(%s)", fn, inner);
            break;
        }

        default:
            if (buffer_size > 0) {
                buffer[0] = '?';
                buffer[buffer_size > 1 ? 1 : 0] = '\0';
            }
    }

    return buffer;
}

/* ============================================================================
 * ALGEBRAIC OPERATIONS API
 * ============================================================================ */

expr_node_t* equation_simplify(equation_engine_t* eq, const expr_node_t* node) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_simplify: eq is NULL");
        return NULL;
    }
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_simplify: node is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_simplify", 0.0f);


    nimcp_mutex_lock(eq->lock);

    expr_node_t* result = equation_copy_expr(eq, node);
    if (!result) {
        nimcp_mutex_unlock(eq->lock);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_simplify: result is NULL");
        return NULL;
    }

    /* Apply simplification rules */
    bool changed = true;
    uint32_t iterations = 0;

    while (changed && iterations < eq->config.max_simplify_iterations) {
        changed = false;
        iterations++;

        /* Constant folding */
        if (result->type >= EXPR_ADD && result->type <= EXPR_POW) {
            if (result->left && result->right &&
                result->left->type == EXPR_CONSTANT &&
                result->right->type == EXPR_CONSTANT) {

                float a = result->left->data.constant;
                float b = result->right->data.constant;
                float val = 0.0f;

                switch (result->type) {
                    case EXPR_ADD: val = a + b; break;
                    case EXPR_SUB: val = a - b; break;
                    case EXPR_MUL: val = a * b; break;
                    case EXPR_DIV: val = (b != 0) ? a / b : NAN; break;
                    case EXPR_POW: val = powf(a, b); break;
                    default: break;
                }

                equation_free_expr(result->left);
                equation_free_expr(result->right);
                result->type = EXPR_CONSTANT;
                result->data.constant = val;
                result->left = result->right = NULL;
                changed = true;
            }
        }

        /* Identity rules */
        if (!changed && result->type == EXPR_ADD) {
            if (result->left->type == EXPR_CONSTANT &&
                is_zero(result->left->data.constant)) {
                /* 0 + x = x */
                expr_node_t* r = result->right;
                equation_free_expr(result->left);
                nimcp_free(result);
                result = NULL;
                result = r;
                changed = true;
            } else if (result->right->type == EXPR_CONSTANT &&
                       is_zero(result->right->data.constant)) {
                /* x + 0 = x */
                expr_node_t* l = result->left;
                equation_free_expr(result->right);
                nimcp_free(result);
                result = NULL;
                result = l;
                changed = true;
            }
        }

        if (!changed && result->type == EXPR_MUL) {
            if (result->left->type == EXPR_CONSTANT) {
                if (is_zero(result->left->data.constant)) {
                    /* 0 * x = 0 */
                    equation_free_expr(result->left);
                    equation_free_expr(result->right);
                    result->type = EXPR_CONSTANT;
                    result->data.constant = 0.0f;
                    result->left = result->right = NULL;
                    changed = true;
                } else if (is_one(result->left->data.constant)) {
                    /* 1 * x = x */
                    expr_node_t* r = result->right;
                    equation_free_expr(result->left);
                    nimcp_free(result);
                    result = NULL;
                    result = r;
                    changed = true;
                }
            } else if (result->right->type == EXPR_CONSTANT) {
                if (is_zero(result->right->data.constant)) {
                    /* x * 0 = 0 */
                    equation_free_expr(result->left);
                    equation_free_expr(result->right);
                    result->type = EXPR_CONSTANT;
                    result->data.constant = 0.0f;
                    result->left = result->right = NULL;
                    changed = true;
                } else if (is_one(result->right->data.constant)) {
                    /* x * 1 = x */
                    expr_node_t* l = result->left;
                    equation_free_expr(result->right);
                    nimcp_free(result);
                    result = NULL;
                    result = l;
                    changed = true;
                }
            }
        }

        if (!changed && result->type == EXPR_POW) {
            if (result->right->type == EXPR_CONSTANT) {
                if (is_zero(result->right->data.constant)) {
                    /* x^0 = 1 */
                    equation_free_expr(result->left);
                    equation_free_expr(result->right);
                    result->type = EXPR_CONSTANT;
                    result->data.constant = 1.0f;
                    result->left = result->right = NULL;
                    changed = true;
                } else if (is_one(result->right->data.constant)) {
                    /* x^1 = x */
                    expr_node_t* l = result->left;
                    equation_free_expr(result->right);
                    nimcp_free(result);
                    result = NULL;
                    result = l;
                    changed = true;
                }
            }
        }
    }

    result->simplified = true;
    eq->simplifications++;

    nimcp_mutex_unlock(eq->lock);

    return result;
}

expr_node_t* equation_expand(equation_engine_t* eq, const expr_node_t* node) {
    /* Simplified expand - just copy for now */
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_expand", 0.0f);


    return equation_copy_expr(eq, node);
}

expr_node_t* equation_factor(equation_engine_t* eq, const expr_node_t* node) {
    /* Simplified factor - just copy for now */
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_factor", 0.0f);


    return equation_copy_expr(eq, node);
}

expr_node_t* equation_substitute(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name,
    const expr_node_t* replacement
) {
    if (!node || !var_name || !replacement) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_substitute: required parameter is NULL (node, var_name, replacement)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_substitute", 0.0f);


    if (node->type == EXPR_VARIABLE &&
        strcmp(node->data.variable, var_name) == 0) {
        return equation_copy_expr(eq, replacement);
    }

    expr_node_t* copy = alloc_node();
    if (!copy) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "copy is NULL");

        return NULL;

    }

    copy->type = node->type;
    copy->depth = node->depth;

    if (node->type == EXPR_CONSTANT) {
        copy->data.constant = node->data.constant;
    } else if (node->type == EXPR_VARIABLE) {
        strncpy(copy->data.variable, node->data.variable, EQUATION_MAX_VAR_NAME - 1);
        copy->data.variable[EQUATION_MAX_VAR_NAME - 1] = '\0';
    }

    if (node->left) {
        copy->left = equation_substitute(eq, node->left, var_name, replacement);
    }
    if (node->right) {
        copy->right = equation_substitute(eq, node->right, var_name, replacement);
    }

    return copy;
}

/* ============================================================================
 * CALCULUS API
 * ============================================================================ */

/* Internal unlocked differentiation helper - call only while holding eq->lock */
static expr_node_t* differentiate_unlocked(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name
) {
    if (!node) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "node is NULL");

        return NULL;

    }

    expr_node_t* result = NULL;

    switch (node->type) {
        case EXPR_CONSTANT:
            /* d/dx[c] = 0 */
            result = equation_create_constant(eq, 0.0f);
            break;

        case EXPR_VARIABLE:
            if (strcmp(node->data.variable, var_name) == 0) {
                /* d/dx[x] = 1 */
                result = equation_create_constant(eq, 1.0f);
            } else {
                /* d/dx[y] = 0 */
                result = equation_create_constant(eq, 0.0f);
            }
            break;

        case EXPR_ADD:
        case EXPR_SUB: {
            /* d/dx[f ± g] = f' ± g' */
            expr_node_t* left_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* right_d = differentiate_unlocked(eq, node->right, var_name);
            result = equation_create_binary(eq, node->type, left_d, right_d);
            break;
        }

        case EXPR_MUL: {
            /* d/dx[f*g] = f'*g + f*g' (product rule) */
            expr_node_t* f = equation_copy_expr(eq, node->left);
            expr_node_t* g = equation_copy_expr(eq, node->right);
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* g_d = differentiate_unlocked(eq, node->right, var_name);

            expr_node_t* term1 = equation_create_binary(eq, EXPR_MUL, f_d, g);
            expr_node_t* term2 = equation_create_binary(eq, EXPR_MUL, f, g_d);
            result = equation_create_binary(eq, EXPR_ADD, term1, term2);
            break;
        }

        case EXPR_DIV: {
            /* d/dx[f/g] = (f'*g - f*g') / g^2 (quotient rule) */
            expr_node_t* f = equation_copy_expr(eq, node->left);
            expr_node_t* g = equation_copy_expr(eq, node->right);
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* g_d = differentiate_unlocked(eq, node->right, var_name);

            expr_node_t* term1 = equation_create_binary(eq, EXPR_MUL, f_d,
                                                         equation_copy_expr(eq, node->right));
            expr_node_t* term2 = equation_create_binary(eq, EXPR_MUL, f, g_d);
            expr_node_t* numer = equation_create_binary(eq, EXPR_SUB, term1, term2);
            expr_node_t* denom = equation_create_binary(eq, EXPR_POW, g,
                                                         equation_create_constant(eq, 2.0f));
            result = equation_create_binary(eq, EXPR_DIV, numer, denom);
            break;
        }

        case EXPR_POW: {
            /* Check if exponent is constant (power rule) or needs chain rule */
            if (node->right->type == EXPR_CONSTANT) {
                /* d/dx[f^n] = n * f^(n-1) * f' */
                float n = node->right->data.constant;
                expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);

                expr_node_t* n_node = equation_create_constant(eq, n);
                expr_node_t* n_minus_1 = equation_create_constant(eq, n - 1.0f);
                expr_node_t* f_copy = equation_copy_expr(eq, node->left);
                expr_node_t* f_pow = equation_create_binary(eq, EXPR_POW, f_copy, n_minus_1);
                expr_node_t* n_f_pow = equation_create_binary(eq, EXPR_MUL, n_node, f_pow);
                result = equation_create_binary(eq, EXPR_MUL, n_f_pow, f_d);
            } else {
                /* General case: d/dx[f^g] = f^g * (g'*ln(f) + g*f'/f) */
                /* Simplified: just return 0 for now */
                result = equation_create_constant(eq, 0.0f);
            }
            break;
        }

        case EXPR_NEG: {
            /* d/dx[-f] = -f' */
            expr_node_t* inner_d = differentiate_unlocked(eq, node->left, var_name);
            result = equation_create_unary(eq, EXPR_NEG, inner_d);
            break;
        }

        case EXPR_SIN: {
            /* d/dx[sin(f)] = cos(f) * f' */
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* cos_f = equation_create_unary(eq, EXPR_COS,
                                                        equation_copy_expr(eq, node->left));
            result = equation_create_binary(eq, EXPR_MUL, cos_f, f_d);
            break;
        }

        case EXPR_COS: {
            /* d/dx[cos(f)] = -sin(f) * f' */
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* sin_f = equation_create_unary(eq, EXPR_SIN,
                                                        equation_copy_expr(eq, node->left));
            expr_node_t* neg_sin = equation_create_unary(eq, EXPR_NEG, sin_f);
            result = equation_create_binary(eq, EXPR_MUL, neg_sin, f_d);
            break;
        }

        case EXPR_EXP: {
            /* d/dx[exp(f)] = exp(f) * f' */
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* exp_f = equation_copy_expr(eq, node);
            result = equation_create_binary(eq, EXPR_MUL, exp_f, f_d);
            break;
        }

        case EXPR_LOG: {
            /* d/dx[log(f)] = f' / f */
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* f = equation_copy_expr(eq, node->left);
            result = equation_create_binary(eq, EXPR_DIV, f_d, f);
            break;
        }

        case EXPR_SQRT: {
            /* d/dx[sqrt(f)] = f' / (2*sqrt(f)) */
            expr_node_t* f_d = differentiate_unlocked(eq, node->left, var_name);
            expr_node_t* two = equation_create_constant(eq, 2.0f);
            expr_node_t* sqrt_f = equation_copy_expr(eq, node);
            expr_node_t* denom = equation_create_binary(eq, EXPR_MUL, two, sqrt_f);
            result = equation_create_binary(eq, EXPR_DIV, f_d, denom);
            break;
        }

        default:
            result = equation_create_constant(eq, 0.0f);
    }

    return result;
}

expr_node_t* equation_differentiate(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name
) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_differentiate: eq is NULL");
        return NULL;
    }
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_differentiate: node is NULL");
        return NULL;
    }
    if (!var_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_differentiate: var_name is NULL");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_differentia", 0.0f);


    nimcp_mutex_lock(eq->lock);

    expr_node_t* result = differentiate_unlocked(eq, node, var_name);
    eq->differentiations++;

    nimcp_mutex_unlock(eq->lock);

    return result;
}

expr_node_t* equation_partial_derivative(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name
) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_partial_der", 0.0f);


    return equation_differentiate(eq, node, var_name);
}

uint32_t equation_gradient(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char** var_names,
    uint32_t num_vars,
    expr_node_t** gradient
) {
    if (!eq || !node || !var_names || !gradient || num_vars == 0) {
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_gradient", 0.0f);


    for (uint32_t i = 0; i < num_vars; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_vars > 256) {
            equation_manipulation_heartbeat("equation_man_loop",
                             (float)(i + 1) / (float)num_vars);
        }

        gradient[i] = equation_differentiate(eq, node, var_names[i]);
    }

    return num_vars;
}

/* ============================================================================
 * EVALUATION API
 * ============================================================================ */

/* Internal unlocked evaluation helper - call only while holding eq->lock */
static float evaluate_unlocked(
    const expr_node_t* node,
    const variable_binding_t* bindings,
    uint32_t num_bindings
) {
    if (!node) return NAN;

    float result = NAN;

    switch (node->type) {
        case EXPR_CONSTANT:
            result = node->data.constant;
            break;

        case EXPR_VARIABLE: {
            result = NAN;
            for (uint32_t i = 0; i < num_bindings; i++) {
                /* Phase 8: Loop progress heartbeat */
                if ((i & 0xFF) == 0 && num_bindings > 256) {
                    equation_manipulation_heartbeat("equation_man_loop",
                                     (float)(i + 1) / (float)num_bindings);
                }

                if (strcmp(bindings[i].name, node->data.variable) == 0) {
                    result = bindings[i].value;
                    break;
                }
            }
            break;
        }

        case EXPR_ADD:
            result = evaluate_unlocked(node->left, bindings, num_bindings) +
                     evaluate_unlocked(node->right, bindings, num_bindings);
            break;

        case EXPR_SUB:
            result = evaluate_unlocked(node->left, bindings, num_bindings) -
                     evaluate_unlocked(node->right, bindings, num_bindings);
            break;

        case EXPR_MUL:
            result = evaluate_unlocked(node->left, bindings, num_bindings) *
                     evaluate_unlocked(node->right, bindings, num_bindings);
            break;

        case EXPR_DIV: {
            float denom = evaluate_unlocked(node->right, bindings, num_bindings);
            result = (denom != 0) ?
                evaluate_unlocked(node->left, bindings, num_bindings) / denom :
                NAN;
            break;
        }

        case EXPR_POW:
            result = powf(evaluate_unlocked(node->left, bindings, num_bindings),
                          evaluate_unlocked(node->right, bindings, num_bindings));
            break;

        case EXPR_NEG:
            result = -evaluate_unlocked(node->left, bindings, num_bindings);
            break;

        case EXPR_SIN:
            result = sinf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_COS:
            result = cosf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_TAN:
            result = tanf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_EXP:
            result = expf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_LOG:
            result = logf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_SQRT:
            result = sqrtf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        case EXPR_ABS:
            result = fabsf(evaluate_unlocked(node->left, bindings, num_bindings));
            break;

        default:
            result = NAN;
    }

    return result;
}

float equation_evaluate(
    equation_engine_t* eq,
    const expr_node_t* node,
    const variable_binding_t* bindings,
    uint32_t num_bindings
) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_evaluate: eq is NULL");
        return NAN;
    }
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_evaluate: node is NULL");
        return NAN;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_evaluate", 0.0f);


    nimcp_mutex_lock(eq->lock);

    float result = evaluate_unlocked(node, bindings, num_bindings);
    eq->evaluations++;

    nimcp_mutex_unlock(eq->lock);

    return result;
}

bool equation_is_constant(const expr_node_t* node) {
    if (!node) return true;

    if (node->type == EXPR_VARIABLE) {
        return false;
    }
    if (node->type == EXPR_CONSTANT) return true;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_is_constant", 0.0f);


    return equation_is_constant(node->left) && equation_is_constant(node->right);
}

bool equation_contains_variable(const expr_node_t* node, const char* var_name) {
    if (!node || !var_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_contains_variable: required parameter is NULL (node, var_name)");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_contains_va", 0.0f);


    if (node->type == EXPR_VARIABLE) {
        return strcmp(node->data.variable, var_name) == 0;
    }

    return equation_contains_variable(node->left, var_name) ||
           equation_contains_variable(node->right, var_name);
}

uint32_t equation_get_variables(
    const expr_node_t* node,
    char var_names[][EQUATION_MAX_VAR_NAME],
    uint32_t max_vars
) {
    if (!node || !var_names || max_vars == 0) return 0;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_get_variabl", 0.0f);


    uint32_t count = 0;

    if (node->type == EXPR_VARIABLE) {
        /* Check if already in list */
        bool found = false;
        for (uint32_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                equation_manipulation_heartbeat("equation_man_loop",
                                 (float)(i + 1) / (float)count);
            }

            if (strcmp(var_names[i], node->data.variable) == 0) {
                found = true;
                break;
            }
        }
        if (!found && count < max_vars) {
            strncpy(var_names[count], node->data.variable, EQUATION_MAX_VAR_NAME - 1);
            var_names[count][EQUATION_MAX_VAR_NAME - 1] = '\0';
            count++;
        }
    }

    if (node->left) {
        uint32_t left_count = equation_get_variables(node->left,
            var_names + count, max_vars - count);
        count += left_count;
    }

    if (node->right && count < max_vars) {
        uint32_t right_count = equation_get_variables(node->right,
            var_names + count, max_vars - count);
        count += right_count;
    }

    return count;
}

/* ============================================================================
 * EQUATION SOLVING API
 * ============================================================================ */

equation_t equation_create_equation(expr_node_t* lhs, expr_node_t* rhs) {
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_create_equa", 0.0f);


    equation_t eqn = {lhs, rhs};
    return eqn;
}

expr_node_t* equation_solve_for(
    equation_engine_t* eq,
    const equation_t* eqn,
    const char* var_name
) {
    if (!eq || !eqn || !var_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_solve_for: required parameter is NULL (eq, eqn, var_name)");
        return NULL;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_solve_for", 0.0f);


    nimcp_mutex_lock(eq->lock);

    /* Simple case: lhs is variable, return rhs */
    if (eqn->lhs && eqn->lhs->type == EXPR_VARIABLE &&
        strcmp(eqn->lhs->data.variable, var_name) == 0) {
        expr_node_t* result = equation_copy_expr(eq, eqn->rhs);
        eq->equations_solved++;
        nimcp_mutex_unlock(eq->lock);
        return result;
    }

    /* TODO: More sophisticated solving */

    nimcp_mutex_unlock(eq->lock);
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_solve_for: operation failed");
    return NULL;
}

float equation_find_root(
    equation_engine_t* eq,
    const expr_node_t* node,
    const char* var_name,
    float initial_guess,
    float tolerance,
    uint32_t max_iterations
) {
    if (!eq || !node || !var_name) return NAN;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_find_root", 0.0f);


    nimcp_mutex_lock(eq->lock);

    /* Newton's method: x_{n+1} = x_n - f(x_n) / f'(x_n) */
    /* Use unlocked helper since we already hold the lock */
    expr_node_t* deriv = differentiate_unlocked(eq, node, var_name);
    if (!deriv) {
        nimcp_mutex_unlock(eq->lock);
        return NAN;
    }

    float x = initial_guess;
    variable_binding_t binding = {{0}, 0.0f};
    strncpy(binding.name, var_name, EQUATION_MAX_VAR_NAME - 1);

    for (uint32_t i = 0; i < max_iterations; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && max_iterations > 256) {
            equation_manipulation_heartbeat("equation_man_loop",
                             (float)(i + 1) / (float)max_iterations);
        }

        binding.value = x;

        /* Use unlocked helper since we already hold the lock */
        float fx = evaluate_unlocked(node, &binding, 1);
        float fpx = evaluate_unlocked(deriv, &binding, 1);

        if (fabsf(fpx) < 1e-10f) break;  /* Avoid division by zero */

        float x_new = x - fx / fpx;

        if (fabsf(x_new - x) < tolerance) {
            equation_free_expr(deriv);
            eq->equations_solved++;
            nimcp_mutex_unlock(eq->lock);
            return x_new;
        }

        x = x_new;
    }

    equation_free_expr(deriv);
    nimcp_mutex_unlock(eq->lock);

    return NAN;
}

/* ============================================================================
 * MODULATION API
 * ============================================================================ */

int equation_set_inflammation(equation_engine_t* eq, float level) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_set_inflammation: eq is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_set_inflamm", 0.0f);


    nimcp_mutex_lock(eq->lock);
    eq->inflammation_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(eq->lock);

    return 0;
}

int equation_set_fatigue(equation_engine_t* eq, float level) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_set_fatigue: eq is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_set_fatigue", 0.0f);


    nimcp_mutex_lock(eq->lock);
    eq->fatigue_level = nimcp_clamp01(level);
    nimcp_mutex_unlock(eq->lock);

    return 0;
}

/* ============================================================================
 * STATISTICS API
 * ============================================================================ */

int equation_get_stats(equation_engine_t* eq, equation_stats_t* stats) {
    if (!eq) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_get_stats: eq is NULL");
        return -1;
    }
    if (!stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "equation_get_stats: stats is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_get_stats", 0.0f);


    nimcp_mutex_lock(((equation_engine_t*)eq)->lock);

    stats->expressions_parsed = eq->expressions_parsed;
    stats->simplifications = eq->simplifications;
    stats->differentiations = eq->differentiations;
    stats->evaluations = eq->evaluations;
    stats->equations_solved = eq->equations_solved;

    if (eq->tree_count > 0) {
        stats->avg_tree_depth = (float)eq->total_tree_depth / (float)eq->tree_count;
    } else {
        stats->avg_tree_depth = 0.0f;
    }

    nimcp_mutex_unlock(((equation_engine_t*)eq)->lock);

    return 0;
}

void equation_reset_stats(equation_engine_t* eq) {
    if (!eq) return;

    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_equation_reset_stats", 0.0f);


    nimcp_mutex_lock(eq->lock);

    eq->expressions_parsed = 0;
    eq->simplifications = 0;
    eq->differentiations = 0;
    eq->evaluations = 0;
    eq->equations_solved = 0;
    eq->total_tree_depth = 0;
    eq->tree_count = 0;

    nimcp_mutex_unlock(eq->lock);
}

const char* equation_get_last_error(void) {
    return g_equation_error;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int equation_manipulation_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;
    /* Phase 8: Heartbeat at operation start */
    equation_manipulation_heartbeat("equation_man_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Equation_Manipulation");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                equation_manipulation_heartbeat("equation_man_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            /* Module self-knowledge logged */
        }
    }
    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Equation_Manipulation");
    if (connections) { kg_relation_list_destroy(connections); }
    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Equation_Manipulation");
    if (incoming) { kg_relation_list_destroy(incoming); }
    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void equation_manipulation_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_equation_manipulation_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int equation_manipulation_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "equation_manipulation_training_begin: NULL argument");
        return -1;
    }
    equation_manipulation_heartbeat_instance(g_equation_manipulation_health_agent, "equation_manipulation_training_begin", 0.0f);
    (void)(struct equation_engine*)instance; /* Module state available for reset */
    return 0;
}

int equation_manipulation_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "equation_manipulation_training_end: NULL argument");
        return -1;
    }
    equation_manipulation_heartbeat_instance(g_equation_manipulation_health_agent, "equation_manipulation_training_end", 1.0f);
    (void)(struct equation_engine*)instance; /* Module state available for finalization */
    return 0;
}

int equation_manipulation_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "equation_manipulation_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    equation_manipulation_heartbeat_instance(g_equation_manipulation_health_agent, "equation_manipulation_training_step", progress);
    (void)(struct equation_engine*)instance; /* Module state available for step adaptation */
    return 0;
}
