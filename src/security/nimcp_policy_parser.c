/**
 * @file nimcp_policy_parser.c
 * @brief NIMCP Policy Parser Implementation
 *
 * WHAT: Recursive descent parser for NIMCP Security Policy Language.
 * WHY:  Converts policy text into AST for compilation and evaluation.
 * HOW:  Tokenizes input, then parses using recursive descent with operator
 *       precedence for expressions.
 */

#include "security/nimcp_policy_ast.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>

#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(policy_parser, MESH_ADAPTER_CATEGORY_SECURITY)


/* ========================================================================
 * Token Types
 * ======================================================================== */

typedef enum {
    TOKEN_EOF,
    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_NUMBER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_POLICY,
    TOKEN_RULE,
    TOKEN_CONDITION,
    TOKEN_ACTION,
    TOKEN_AND,
    TOKEN_OR,
    TOKEN_NOT,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_LBRACE,
    TOKEN_RBRACE,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_COLON,
    TOKEN_COMMA,
    TOKEN_DOT,
    TOKEN_EQ,
    TOKEN_NE,
    TOKEN_LT,
    TOKEN_LE,
    TOKEN_GT,
    TOKEN_GE,
    TOKEN_ASSIGN,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_MULTIPLY,
    TOKEN_DIVIDE,
    TOKEN_MOD,
    TOKEN_ERROR
} token_type_t;

typedef struct {
    token_type_t type;
    char* value;
    size_t line;
    size_t column;
} token_t;

typedef struct {
    const char* input;
    const char* current;
    const char* filename;
    size_t line;
    size_t column;
    token_t current_token;
    bool has_error;
    char error_message[NIMCP_ERROR_BUFFER_SIZE];
} parser_state_t;

/* ========================================================================
 * Lexer
 * ======================================================================== */

static void skip_whitespace(parser_state_t* state) {
    while (*state->current && isspace(*state->current)) {
        if (*state->current == '\n') {
            state->line++;
            state->column = 0;
        } else {
            state->column++;
        }
        state->current++;
    }
}

static void skip_comment(parser_state_t* state) {
    if (*state->current == '#') {
        while (*state->current && *state->current != '\n') {
            state->current++;
        }
        if (*state->current == '\n') {
            state->line++;
            state->column = 0;
            state->current++;
        }
    }
}

static bool is_identifier_start(char c) {
    return isalpha(c) || c == '_';
}

static bool is_identifier_char(char c) {
    return isalnum(c) || c == '_';
}

static char* read_identifier(parser_state_t* state) {
    const char* start = state->current;
    while (is_identifier_char(*state->current)) {
        state->current++;
        state->column++;
    }
    size_t len = state->current - start;
    char* result = strndup(start, len);
    return result;
}

static char* read_string(parser_state_t* state) {
    state->current++; // Skip opening quote
    state->column++;

    const char* start = state->current;
    while (*state->current && *state->current != '"') {
        if (*state->current == '\\' && *(state->current + 1)) {
            state->current += 2;
            state->column += 2;
        } else {
            state->current++;
            state->column++;
        }
    }

    if (*state->current != '"') {
        snprintf(state->error_message, sizeof(state->error_message),
                 "Unterminated string at line %zu", state->line);
        state->has_error = true;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "read_string: validation failed");
        return NULL;
    }

    size_t len = state->current - start;
    char* result = strndup(start, len);

    state->current++; // Skip closing quote
    state->column++;

    return result;
}

static char* read_number(parser_state_t* state) {
    const char* start = state->current;

    if (*state->current == '-') {
        state->current++;
        state->column++;
    }

    while (isdigit(*state->current)) {
        state->current++;
        state->column++;
    }

    if (*state->current == '.') {
        state->current++;
        state->column++;
        while (isdigit(*state->current)) {
            state->current++;
            state->column++;
        }
    }

    size_t len = state->current - start;
    char* result = strndup(start, len);
    return result;
}

static token_t next_token(parser_state_t* state) {
    token_t token = {0};

    while (true) {
        skip_whitespace(state);
        if (*state->current == '#') {
            skip_comment(state);
        } else {
            break;
        }
    }

    token.line = state->line;
    token.column = state->column;

    if (!*state->current) {
        token.type = TOKEN_EOF;
        return token;
    }

    // Two-character operators
    if (state->current[0] == '=' && state->current[1] == '=') {
        token.type = TOKEN_EQ;
        state->current += 2;
        state->column += 2;
        return token;
    }
    if (state->current[0] == '!' && state->current[1] == '=') {
        token.type = TOKEN_NE;
        state->current += 2;
        state->column += 2;
        return token;
    }
    if (state->current[0] == '<' && state->current[1] == '=') {
        token.type = TOKEN_LE;
        state->current += 2;
        state->column += 2;
        return token;
    }
    if (state->current[0] == '>' && state->current[1] == '=') {
        token.type = TOKEN_GE;
        state->current += 2;
        state->column += 2;
        return token;
    }

    // Single-character operators
    switch (*state->current) {
        case '(':
            token.type = TOKEN_LPAREN;
            state->current++;
            state->column++;
            return token;
        case ')':
            token.type = TOKEN_RPAREN;
            state->current++;
            state->column++;
            return token;
        case '{':
            token.type = TOKEN_LBRACE;
            state->current++;
            state->column++;
            return token;
        case '}':
            token.type = TOKEN_RBRACE;
            state->current++;
            state->column++;
            return token;
        case '[':
            token.type = TOKEN_LBRACKET;
            state->current++;
            state->column++;
            return token;
        case ']':
            token.type = TOKEN_RBRACKET;
            state->current++;
            state->column++;
            return token;
        case ':':
            token.type = TOKEN_COLON;
            state->current++;
            state->column++;
            return token;
        case ',':
            token.type = TOKEN_COMMA;
            state->current++;
            state->column++;
            return token;
        case '.':
            token.type = TOKEN_DOT;
            state->current++;
            state->column++;
            return token;
        case '<':
            token.type = TOKEN_LT;
            state->current++;
            state->column++;
            return token;
        case '>':
            token.type = TOKEN_GT;
            state->current++;
            state->column++;
            return token;
        case '+':
            token.type = TOKEN_PLUS;
            state->current++;
            state->column++;
            return token;
        case '-':
            if (isdigit(state->current[1])) {
                token.type = TOKEN_NUMBER;
                token.value = read_number(state);
                return token;
            }
            token.type = TOKEN_MINUS;
            state->current++;
            state->column++;
            return token;
        case '*':
            token.type = TOKEN_MULTIPLY;
            state->current++;
            state->column++;
            return token;
        case '/':
            token.type = TOKEN_DIVIDE;
            state->current++;
            state->column++;
            return token;
        case '%':
            token.type = TOKEN_MOD;
            state->current++;
            state->column++;
            return token;
    }

    // String
    if (*state->current == '"') {
        token.type = TOKEN_STRING;
        token.value = read_string(state);
        if (state->has_error) {
            token.type = TOKEN_ERROR;
        }
        return token;
    }

    // Number
    if (isdigit(*state->current)) {
        token.type = TOKEN_NUMBER;
        token.value = read_number(state);
        return token;
    }

    // Identifier or keyword
    if (is_identifier_start(*state->current)) {
        token.value = read_identifier(state);

        // Check for keywords
        if (strcmp(token.value, "policy") == 0) {
            token.type = TOKEN_POLICY;
        } else if (strcmp(token.value, "rule") == 0) {
            token.type = TOKEN_RULE;
        } else if (strcmp(token.value, "condition") == 0) {
            token.type = TOKEN_CONDITION;
        } else if (strcmp(token.value, "action") == 0) {
            token.type = TOKEN_ACTION;
        } else if (strcmp(token.value, "AND") == 0) {
            token.type = TOKEN_AND;
        } else if (strcmp(token.value, "OR") == 0) {
            token.type = TOKEN_OR;
        } else if (strcmp(token.value, "NOT") == 0) {
            token.type = TOKEN_NOT;
        } else if (strcmp(token.value, "true") == 0) {
            token.type = TOKEN_TRUE;
        } else if (strcmp(token.value, "false") == 0) {
            token.type = TOKEN_FALSE;
        } else {
            token.type = TOKEN_IDENTIFIER;
        }

        return token;
    }

    // Unknown character
    snprintf(state->error_message, sizeof(state->error_message),
             "Unexpected character '%c' at line %zu, column %zu",
             *state->current, state->line, state->column);
    state->has_error = true;
    token.type = TOKEN_ERROR;
    return token;
}

static void advance(parser_state_t* state) {
    if (state->current_token.value) {
        nimcp_free(state->current_token.value);
    }
    state->current_token = next_token(state);
}

static bool match(parser_state_t* state, token_type_t type) {
    return state->current_token.type == type;
}

static bool expect(parser_state_t* state, token_type_t type, const char* msg) {
    if (!match(state, type)) {
        snprintf(state->error_message, sizeof(state->error_message),
                 "%s at line %zu, column %zu",
                 msg, state->current_token.line, state->current_token.column);
        state->has_error = true;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "expect: match is NULL");
        return false;
    }
    return true;
}

/* ========================================================================
 * Parser Functions
 * ======================================================================== */

static nimcp_ast_node_t* parse_expression(parser_state_t* state);
static nimcp_ast_node_t* parse_primary(parser_state_t* state);

static nimcp_ast_node_t* parse_primary(parser_state_t* state) {
    nimcp_ast_node_t* node = NULL;

    if (match(state, TOKEN_STRING)) {
        node = nimcp_ast_create_literal_string(state->current_token.value);
        advance(state);
    } else if (match(state, TOKEN_NUMBER)) {
        if (strchr(state->current_token.value, '.')) {
            // P1-2 fix: Use strtod instead of atof for safe conversion
            char* endptr;
            errno = 0;
            double val = strtod(state->current_token.value, &endptr);
            if (endptr == state->current_token.value || errno == ERANGE) {
                val = 0.0;
            }
            node = nimcp_ast_create_literal_float(val);
        } else {
            // P1-2 fix: Use strtoll instead of atoll with error checking
            char* endptr;
            errno = 0;
            long long val = strtoll(state->current_token.value, &endptr, 10);
            if (endptr == state->current_token.value || errno == ERANGE) {
                val = 0;
            }
            node = nimcp_ast_create_literal_int(val);
        }
        advance(state);
    } else if (match(state, TOKEN_TRUE)) {
        node = nimcp_ast_create_literal_bool(true);
        advance(state);
    } else if (match(state, TOKEN_FALSE)) {
        node = nimcp_ast_create_literal_bool(false);
        advance(state);
    } else if (match(state, TOKEN_IDENTIFIER)) {
        char* name = strdup(state->current_token.value);
        advance(state);

        // Function call
        if (match(state, TOKEN_LPAREN)) {
            advance(state);

            nimcp_ast_node_t** args = NULL;
            size_t num_args = 0;
            size_t capacity = 0;

            while (!match(state, TOKEN_RPAREN) && !match(state, TOKEN_EOF)) {
                if (num_args >= capacity) {
                    capacity = capacity == 0 ? 4 : capacity * 2;
                    args = nimcp_realloc(args, capacity * sizeof(nimcp_ast_node_t*));
                }
                args[num_args++] = parse_expression(state);

                if (match(state, TOKEN_COMMA)) {
                    advance(state);
                }
            }

            expect(state, TOKEN_RPAREN, "Expected ')'");
            advance(state);

            node = nimcp_ast_create_call(name, args, num_args);
        } else {
            node = nimcp_ast_create_identifier(name);
        }

        nimcp_free(name);

        // Member access
        while (match(state, TOKEN_DOT)) {
            advance(state);
            if (!expect(state, TOKEN_IDENTIFIER, "Expected member name")) {
                nimcp_ast_destroy(node);
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_primary: expect is NULL");
                return NULL;
            }
            node = nimcp_ast_create_member(node, state->current_token.value);
            advance(state);
        }
    } else if (match(state, TOKEN_LPAREN)) {
        advance(state);
        node = parse_expression(state);
        expect(state, TOKEN_RPAREN, "Expected ')'");
        advance(state);
    } else if (match(state, TOKEN_NOT)) {
        advance(state);
        node = nimcp_ast_create_unary(NIMCP_OP_NOT, parse_primary(state));
    } else if (match(state, TOKEN_MINUS)) {
        advance(state);
        node = nimcp_ast_create_unary(NIMCP_OP_NEG, parse_primary(state));
    }

    return node;
}

static nimcp_ast_node_t* parse_multiplicative(parser_state_t* state) {
    nimcp_ast_node_t* left = parse_primary(state);

    while (match(state, TOKEN_MULTIPLY) || match(state, TOKEN_DIVIDE) || match(state, TOKEN_MOD)) {
        nimcp_binary_op_t op;
        if (match(state, TOKEN_MULTIPLY)) op = NIMCP_OP_MUL;
        else if (match(state, TOKEN_DIVIDE)) op = NIMCP_OP_DIV;
        else op = NIMCP_OP_MOD;

        advance(state);
        nimcp_ast_node_t* right = parse_primary(state);
        left = nimcp_ast_create_binary(op, left, right);
    }

    return left;
}

static nimcp_ast_node_t* parse_additive(parser_state_t* state) {
    nimcp_ast_node_t* left = parse_multiplicative(state);

    while (match(state, TOKEN_PLUS) || match(state, TOKEN_MINUS)) {
        nimcp_binary_op_t op = match(state, TOKEN_PLUS) ? NIMCP_OP_ADD : NIMCP_OP_SUB;
        advance(state);
        nimcp_ast_node_t* right = parse_multiplicative(state);
        left = nimcp_ast_create_binary(op, left, right);
    }

    return left;
}

static nimcp_ast_node_t* parse_comparison(parser_state_t* state) {
    nimcp_ast_node_t* left = parse_additive(state);

    while (match(state, TOKEN_LT) || match(state, TOKEN_LE) ||
           match(state, TOKEN_GT) || match(state, TOKEN_GE) ||
           match(state, TOKEN_EQ) || match(state, TOKEN_NE)) {
        nimcp_binary_op_t op;
        if (match(state, TOKEN_LT)) op = NIMCP_OP_LT;
        else if (match(state, TOKEN_LE)) op = NIMCP_OP_LE;
        else if (match(state, TOKEN_GT)) op = NIMCP_OP_GT;
        else if (match(state, TOKEN_GE)) op = NIMCP_OP_GE;
        else if (match(state, TOKEN_EQ)) op = NIMCP_OP_EQ;
        else op = NIMCP_OP_NE;

        advance(state);
        nimcp_ast_node_t* right = parse_additive(state);
        left = nimcp_ast_create_binary(op, left, right);
    }

    return left;
}

static nimcp_ast_node_t* parse_logical_and(parser_state_t* state) {
    nimcp_ast_node_t* left = parse_comparison(state);

    while (match(state, TOKEN_AND)) {
        advance(state);
        nimcp_ast_node_t* right = parse_comparison(state);
        left = nimcp_ast_create_binary(NIMCP_OP_AND, left, right);
    }

    return left;
}

static nimcp_ast_node_t* parse_logical_or(parser_state_t* state) {
    nimcp_ast_node_t* left = parse_logical_and(state);

    while (match(state, TOKEN_OR)) {
        advance(state);
        nimcp_ast_node_t* right = parse_logical_and(state);
        left = nimcp_ast_create_binary(NIMCP_OP_OR, left, right);
    }

    return left;
}

static nimcp_ast_node_t* parse_expression(parser_state_t* state) {
    return parse_logical_or(state);
}

static nimcp_ast_node_t* parse_param(parser_state_t* state) {
    if (!expect(state, TOKEN_IDENTIFIER, "Expected parameter name")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_param: expect is NULL");
        return NULL;
    }

    char* key = strdup(state->current_token.value);
    advance(state);

    if (!expect(state, TOKEN_COLON, "Expected ':'")) {
        nimcp_free(key);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parse_param: expect is NULL");
        return NULL;
    }
    advance(state);

    nimcp_ast_node_t* value = parse_expression(state);
    nimcp_ast_node_t* param = nimcp_ast_create_param(key, value);
    nimcp_free(key);

    return param;
}

static nimcp_ast_node_t* parse_rule(parser_state_t* state) {
    if (!expect(state, TOKEN_RULE, "Expected 'rule'")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_rule: expect is NULL");
        return NULL;
    }
    advance(state);

    char* name = NULL;
    if (match(state, TOKEN_STRING)) {
        name = strdup(state->current_token.value);
        advance(state);
    }

    if (!expect(state, TOKEN_LBRACE, "Expected '{'")) {
        nimcp_free(name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parse_rule: expect is NULL");
        return NULL;
    }
    advance(state);

    nimcp_ast_node_t* condition = NULL;
    nimcp_ast_node_t* action = NULL;
    nimcp_ast_node_t** params = NULL;
    size_t num_params = 0;
    size_t capacity = 0;

    while (!match(state, TOKEN_RBRACE) && !match(state, TOKEN_EOF)) {
        if (match(state, TOKEN_CONDITION)) {
            advance(state);
            expect(state, TOKEN_COLON, "Expected ':'");
            advance(state);
            condition = parse_expression(state);
        } else if (match(state, TOKEN_ACTION)) {
            advance(state);
            expect(state, TOKEN_COLON, "Expected ':'");
            advance(state);

            if (expect(state, TOKEN_IDENTIFIER, "Expected action type")) {
                action = nimcp_ast_create_action(state->current_token.value, NULL, 0);
                advance(state);
            }
        } else if (match(state, TOKEN_IDENTIFIER)) {
            if (num_params >= capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                params = nimcp_realloc(params, capacity * sizeof(nimcp_ast_node_t*));
            }
            params[num_params++] = parse_param(state);
        } else {
            advance(state);
        }
    }

    expect(state, TOKEN_RBRACE, "Expected '}'");
    advance(state);

    nimcp_ast_node_t* rule = nimcp_ast_create_rule(name, condition, action, params, num_params);
    nimcp_free(name);

    return rule;
}

static nimcp_ast_node_t* parse_policy(parser_state_t* state) {
    if (!expect(state, TOKEN_POLICY, "Expected 'policy'")) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "parse_policy: expect is NULL");
        return NULL;
    }
    advance(state);

    char* name = NULL;
    if (match(state, TOKEN_STRING)) {
        name = strdup(state->current_token.value);
        advance(state);
    }

    if (!expect(state, TOKEN_LBRACE, "Expected '{'")) {
        nimcp_free(name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "parse_policy: expect is NULL");
        return NULL;
    }
    advance(state);

    nimcp_ast_node_t** rules = NULL;
    size_t num_rules = 0;
    size_t capacity = 0;

    while (!match(state, TOKEN_RBRACE) && !match(state, TOKEN_EOF)) {
        if (match(state, TOKEN_RULE)) {
            if (num_rules >= capacity) {
                capacity = capacity == 0 ? 4 : capacity * 2;
                rules = nimcp_realloc(rules, capacity * sizeof(nimcp_ast_node_t*));
            }
            rules[num_rules++] = parse_rule(state);
        } else {
            advance(state);
        }
    }

    expect(state, TOKEN_RBRACE, "Expected '}'");
    advance(state);

    nimcp_ast_node_t* policy = nimcp_ast_create_policy(name, rules, num_rules, NULL, 0);
    nimcp_free(name);

    return policy;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

nimcp_ast_node_t* nimcp_policy_parse(const char* input, const char* filename, char** error_msg) {
    LOG_INFO("Parsing policy from %s", filename ? filename : "(string)");

    parser_state_t state = {0};
    state.input = input;
    state.current = input;
    state.filename = filename;
    state.line = 1;
    state.column = 1;
    state.has_error = false;

    // Initialize first token
    advance(&state);

    nimcp_ast_node_t* root = NULL;

    // Parse top-level constructs
    if (match(&state, TOKEN_POLICY)) {
        root = parse_policy(&state);
    } else if (match(&state, TOKEN_RULE)) {
        root = parse_rule(&state);
    } else {
        snprintf(state.error_message, sizeof(state.error_message),
                 "Expected 'policy' or 'rule' at line %zu", state.line);
        state.has_error = true;
    }

    if (state.current_token.value) {
        nimcp_free(state.current_token.value);
    }

    if (state.has_error) {
        LOG_ERROR("Parse error: %s", state.error_message);
        if (error_msg) {
            *error_msg = strdup(state.error_message);
        }
        if (root) {
            nimcp_ast_destroy(root);
        }
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_policy_parse: validation failed");
        return NULL;
    }

    LOG_INFO("Successfully parsed policy");
    return root;
}
