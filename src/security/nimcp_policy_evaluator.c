/**
 * @file nimcp_policy_evaluator.c
 * @brief NIMCP Policy Evaluator - Bytecode Interpreter
 *
 * WHAT: Interprets compiled bytecode to evaluate policies against contexts.
 * WHY:  Provides efficient runtime evaluation of security policies.
 * HOW:  Stack-based virtual machine with built-in function support.
 */

#include "security/nimcp_policy_engine.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(policy_evaluator, MESH_ADAPTER_CATEGORY_SECURITY)


/* Error code aliases for this file */
#ifndef NIMCP_OK
#define NIMCP_OK NIMCP_SUCCESS
#endif
#ifndef NIMCP_ERROR_INVALID_PARAM
#define NIMCP_ERROR_INVALID_PARAM NIMCP_ERROR_INVALID_PARAM
#endif
#ifndef NIMCP_ERROR_NOT_FOUND
#define NIMCP_ERROR_NOT_FOUND (-120)
#endif
#ifndef NIMCP_ERROR_NO_MEMORY
#define NIMCP_ERROR_NO_MEMORY NIMCP_ERROR_MEMORY
#endif

/* Import bytecode types from compiler */
typedef enum {
    OP_PUSH_INT, OP_PUSH_FLOAT, OP_PUSH_STRING, OP_PUSH_BOOL,
    OP_LOAD_VAR, OP_LOAD_MEMBER, OP_CALL,
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NE, OP_LT, OP_LE, OP_GT, OP_GE,
    OP_AND, OP_OR, OP_NOT, OP_NEG,
    OP_JUMP_IF_FALSE, OP_JUMP, OP_RETURN, OP_ACTION
} opcode_t;

typedef struct {
    opcode_t opcode;
    union {
        int64_t int_val;
        double float_val;
        char* string_val;
        bool bool_val;
        size_t index;
    };
} instruction_t;

typedef struct {
    instruction_t* instructions;
    size_t count;
    size_t capacity;
    char** string_pool;
    size_t string_count;
} bytecode_t;

extern bytecode_t* nimcp_policy_compile(const void* ast);
extern void nimcp_policy_bytecode_destroy(bytecode_t* bc);

/* ========================================================================
 * Value Stack
 * ======================================================================== */

typedef struct {
    nimcp_policy_value_t* values;
    size_t count;
    size_t capacity;
} value_stack_t;

static value_stack_t* stack_create(void) {
    value_stack_t* stack = nimcp_calloc(1, sizeof(value_stack_t));
    if (!stack) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stack is NULL");

        return NULL;

    }

    stack->capacity = 256;
    stack->values = nimcp_calloc(stack->capacity, sizeof(nimcp_policy_value_t));
    if (!stack->values) {
        nimcp_free(stack);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "stack_create: stack->values is NULL");
        return NULL;
    }

    return stack;
}

static void stack_destroy(value_stack_t* stack) {
    if (!stack) return;

    for (size_t i = 0; i < stack->count; i++) {
        if (stack->values[i].type == NIMCP_POLICY_VALUE_STRING) {
            nimcp_free(stack->values[i].string_val);
        }
    }
    nimcp_free(stack->values);
    nimcp_free(stack);
}

static void stack_push(value_stack_t* stack, nimcp_policy_value_t value) {
    if (stack->count >= stack->capacity) {
        size_t new_capacity = stack->capacity * 2;
        // Guard against integer overflow
        if (new_capacity < stack->capacity || new_capacity > SIZE_MAX / sizeof(nimcp_policy_value_t)) {
            return;  // Overflow, cannot grow
        }
        nimcp_policy_value_t* new_values = nimcp_realloc(stack->values,
                                                    new_capacity * sizeof(nimcp_policy_value_t));
        if (!new_values) {
            return;  // Keep original buffer intact
        }
        stack->values = new_values;
        stack->capacity = new_capacity;
    }
    stack->values[stack->count++] = value;
}

static nimcp_policy_value_t stack_pop(value_stack_t* stack) {
    if (stack->count == 0) {
        nimcp_policy_value_t null_val = {0};
        null_val.type = NIMCP_POLICY_VALUE_NULL;
        return null_val;
    }
    return stack->values[--stack->count];
}

static nimcp_policy_value_t stack_peek(value_stack_t* stack) {
    if (stack->count == 0) {
        nimcp_policy_value_t null_val = {0};
        null_val.type = NIMCP_POLICY_VALUE_NULL;
        return null_val;
    }
    return stack->values[stack->count - 1];
}

/* ========================================================================
 * Context Implementation
 * ======================================================================== */

#define CONTEXT_MAGIC 0x43545854  /* 'CTXT' */

typedef struct context_entry {
    char* key;
    nimcp_policy_value_t value;
    struct context_entry* next;
} context_entry_t;

struct nimcp_policy_context {
    uint32_t magic;
    context_entry_t** buckets;
    size_t bucket_count;
    size_t entry_count;
};

static uint32_t hash_string(const char* str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

nimcp_policy_context_t nimcp_policy_context_create(void) {
    struct nimcp_policy_context* ctx = nimcp_calloc(1, sizeof(struct nimcp_policy_context));
    if (!ctx) {
        LOG_ERROR("Failed to allocate context");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_policy_context_create: ctx is NULL");
        return NULL;
    }

    ctx->magic = CONTEXT_MAGIC;
    ctx->bucket_count = 32;
    ctx->buckets = nimcp_calloc(ctx->bucket_count, sizeof(context_entry_t*));

    if (!ctx->buckets) {
        nimcp_free(ctx);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_policy_context_create: ctx->buckets is NULL");
        return NULL;
    }

    LOG_DEBUG("Created policy context");
    return ctx;
}

void nimcp_policy_context_destroy(nimcp_policy_context_t ctx) {
    if (!ctx || ctx->magic != CONTEXT_MAGIC) return;

    for (size_t i = 0; i < ctx->bucket_count; i++) {
        context_entry_t* entry = ctx->buckets[i];
        while (entry) {
            context_entry_t* next = entry->next;
            nimcp_free(entry->key);
            if (entry->value.type == NIMCP_POLICY_VALUE_STRING) {
                nimcp_free(entry->value.string_val);
            }
            nimcp_free(entry);
            entry = next;
        }
    }

    nimcp_free(ctx->buckets);
    ctx->magic = 0;
    nimcp_free(ctx);

    LOG_DEBUG("Destroyed policy context");
}

static nimcp_error_t context_set(
    nimcp_policy_context_t ctx,
    const char* key,
    nimcp_policy_value_t value)
{
    if (!ctx || ctx->magic != CONTEXT_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint32_t hash = hash_string(key);
    size_t bucket = hash % ctx->bucket_count;

    // Check if key exists
    context_entry_t* entry = ctx->buckets[bucket];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Update existing
            if (entry->value.type == NIMCP_POLICY_VALUE_STRING) {
                nimcp_free(entry->value.string_val);
            }
            entry->value = value;
            return NIMCP_OK;
        }
        entry = entry->next;
    }

    // Create new entry
    entry = nimcp_calloc(1, sizeof(context_entry_t));
    if (!entry) return NIMCP_ERROR_NO_MEMORY;

    entry->key = strdup(key);
    if (!entry->key) {
        nimcp_free(entry);
        return NIMCP_ERROR_NO_MEMORY;
    }
    entry->value = value;
    entry->next = ctx->buckets[bucket];
    ctx->buckets[bucket] = entry;
    ctx->entry_count++;

    return NIMCP_OK;
}

nimcp_error_t nimcp_policy_context_set_string(
    nimcp_policy_context_t ctx,
    const char* key,
    const char* value)
{
    nimcp_policy_value_t val = {0};
    val.type = NIMCP_POLICY_VALUE_STRING;
    val.string_val = strdup(value);
    if (!val.string_val) {
        return NIMCP_ERROR_NO_MEMORY;
    }
    return context_set(ctx, key, val);
}

nimcp_error_t nimcp_policy_context_set_int(
    nimcp_policy_context_t ctx,
    const char* key,
    int64_t value)
{
    nimcp_policy_value_t val = {0};
    val.type = NIMCP_POLICY_VALUE_INT;
    val.int_val = value;
    return context_set(ctx, key, val);
}

nimcp_error_t nimcp_policy_context_set_float(
    nimcp_policy_context_t ctx,
    const char* key,
    double value)
{
    nimcp_policy_value_t val = {0};
    val.type = NIMCP_POLICY_VALUE_FLOAT;
    val.float_val = value;
    return context_set(ctx, key, val);
}

nimcp_error_t nimcp_policy_context_set_bool(
    nimcp_policy_context_t ctx,
    const char* key,
    bool value)
{
    nimcp_policy_value_t val = {0};
    val.type = NIMCP_POLICY_VALUE_BOOL;
    val.bool_val = value;
    return context_set(ctx, key, val);
}

nimcp_error_t nimcp_policy_context_get(
    nimcp_policy_context_t ctx,
    const char* key,
    nimcp_policy_value_t* value)
{
    if (!ctx || ctx->magic != CONTEXT_MAGIC || !key || !value) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    uint32_t hash = hash_string(key);
    size_t bucket = hash % ctx->bucket_count;

    context_entry_t* entry = ctx->buckets[bucket];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            *value = entry->value;
            return NIMCP_OK;
        }
        entry = entry->next;
    }

    return NIMCP_ERROR_NOT_FOUND;
}

/* ========================================================================
 * Built-in Functions
 * ======================================================================== */

static nimcp_error_t builtin_contains(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    (void)user_data;

    if (num_args != 2) {
        LOG_ERROR("contains() expects 2 arguments, got %zu", num_args);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (args[0].type != NIMCP_POLICY_VALUE_STRING ||
        args[1].type != NIMCP_POLICY_VALUE_STRING) {
        LOG_ERROR("contains() expects string arguments");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    result->type = NIMCP_POLICY_VALUE_BOOL;
    result->bool_val = (strstr(args[0].string_val, args[1].string_val) != NULL);

    return NIMCP_OK;
}

static nimcp_error_t builtin_length(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    (void)user_data;

    if (num_args != 1) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (args[0].type != NIMCP_POLICY_VALUE_STRING) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    result->type = NIMCP_POLICY_VALUE_INT;
    result->int_val = strlen(args[0].string_val);

    return NIMCP_OK;
}

static nimcp_error_t builtin_entropy(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    (void)user_data;

    if (num_args != 1 || args[0].type != NIMCP_POLICY_VALUE_STRING) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    const char* str = args[0].string_val;
    size_t len = strlen(str);

    if (len == 0) {
        result->type = NIMCP_POLICY_VALUE_FLOAT;
        result->float_val = 0.0;
        return NIMCP_OK;
    }

    // Calculate Shannon entropy
    int freq[256] = {0};
    for (size_t i = 0; i < len; i++) {
        freq[(unsigned char)str[i]]++;
    }

    double entropy = 0.0;
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            double p = (double)freq[i] / len;
            entropy -= p * log2(p);
        }
    }

    result->type = NIMCP_POLICY_VALUE_FLOAT;
    result->float_val = entropy / 8.0; // Normalize to 0-1
    return NIMCP_OK;
}

static nimcp_error_t builtin_matches(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data)
{
    (void)user_data;

    if (num_args != 2 ||
        args[0].type != NIMCP_POLICY_VALUE_STRING ||
        args[1].type != NIMCP_POLICY_VALUE_STRING) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Simple wildcard matching (not full regex)
    const char* str = args[0].string_val;
    const char* pattern = args[1].string_val;

    result->type = NIMCP_POLICY_VALUE_BOOL;
    result->bool_val = (strstr(str, pattern) != NULL);

    return NIMCP_OK;
}

/* ========================================================================
 * Function Registry
 * ======================================================================== */

typedef struct {
    char* name;
    nimcp_policy_function_t func;
    void* user_data;
} function_entry_t;

typedef struct {
    function_entry_t* functions;
    size_t count;
    size_t capacity;
} function_registry_t;

/* Non-static - called from nimcp_policy_engine.c */
function_registry_t* registry_create(void) {
    function_registry_t* registry = nimcp_calloc(1, sizeof(function_registry_t));
    if (!registry) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "registry is NULL");

        return NULL;

    }

    registry->capacity = 16;
    registry->functions = nimcp_calloc(registry->capacity, sizeof(function_entry_t));

    // Register built-in functions
    function_entry_t builtins[] = {
        {"contains", builtin_contains, NULL},
        {"length", builtin_length, NULL},
        {"entropy", builtin_entropy, NULL},
        {"matches", builtin_matches, NULL}
    };

    for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        char* name_copy = strdup(builtins[i].name);
        if (!name_copy) {
            // Cleanup already-registered functions on failure
            for (size_t j = 0; j < registry->count; j++) {
                nimcp_free(registry->functions[j].name);
            }
            nimcp_free(registry->functions);
            nimcp_free(registry);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "registry_create: name_copy is NULL");
            return NULL;
        }
        registry->functions[registry->count].name = name_copy;
        registry->functions[registry->count].func = builtins[i].func;
        registry->functions[registry->count].user_data = builtins[i].user_data;
        registry->count++;
    }

    return registry;
}

/* Non-static - called from nimcp_policy_engine.c */
void registry_destroy(function_registry_t* registry) {
    if (!registry) return;

    for (size_t i = 0; i < registry->count; i++) {
        nimcp_free(registry->functions[i].name);
    }
    nimcp_free(registry->functions);
    nimcp_free(registry);
}

static nimcp_policy_function_t registry_lookup(
    function_registry_t* registry,
    const char* name,
    void** user_data)
{
    for (size_t i = 0; i < registry->count; i++) {
        if (strcmp(registry->functions[i].name, name) == 0) {
            if (user_data) {
                *user_data = registry->functions[i].user_data;
            }
            return registry->functions[i].func;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "registry_lookup: validation failed");
    return NULL;
}

/* ========================================================================
 * Evaluator
 * ======================================================================== */

static bool value_to_bool(const nimcp_policy_value_t* val) {
    switch (val->type) {
        case NIMCP_POLICY_VALUE_BOOL:
            return val->bool_val;
        case NIMCP_POLICY_VALUE_INT:
            return val->int_val != 0;
        case NIMCP_POLICY_VALUE_FLOAT:
            return val->float_val != 0.0;
        case NIMCP_POLICY_VALUE_STRING:
            return val->string_val && val->string_val[0] != '\0';
        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "value_to_bool: operation failed");
            return false;
    }
}

nimcp_error_t nimcp_policy_evaluate_bytecode(
    bytecode_t* bc,
    nimcp_policy_context_t ctx,
    function_registry_t* registry,
    nimcp_policy_result_t* result)
{
    if (!bc || !ctx || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    struct timespec start_time, end_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);

    value_stack_t* stack = stack_create();
    if (!stack) {
        return NIMCP_ERROR_NO_MEMORY;
    }

    size_t pc = 0;  // Program counter
    nimcp_error_t error = NIMCP_OK;

    while (pc < bc->count) {
        instruction_t instr = bc->instructions[pc++];

        switch (instr.opcode) {
            case OP_PUSH_INT: {
                nimcp_policy_value_t val = {0};
                val.type = NIMCP_POLICY_VALUE_INT;
                val.int_val = instr.int_val;
                stack_push(stack, val);
                break;
            }

            case OP_PUSH_FLOAT: {
                nimcp_policy_value_t val = {0};
                val.type = NIMCP_POLICY_VALUE_FLOAT;
                val.float_val = instr.float_val;
                stack_push(stack, val);
                break;
            }

            case OP_PUSH_STRING: {
                nimcp_policy_value_t val = {0};
                val.type = NIMCP_POLICY_VALUE_STRING;
                val.string_val = strdup(bc->string_pool[instr.index]);
                if (!val.string_val) {
                    error = NIMCP_ERROR_NO_MEMORY;
                    goto cleanup;
                }
                stack_push(stack, val);
                break;
            }

            case OP_PUSH_BOOL: {
                nimcp_policy_value_t val = {0};
                val.type = NIMCP_POLICY_VALUE_BOOL;
                val.bool_val = instr.bool_val;
                stack_push(stack, val);
                break;
            }

            case OP_LOAD_VAR: {
                nimcp_policy_value_t val = {0};
                const char* var_name = bc->string_pool[instr.index];
                if (nimcp_policy_context_get(ctx, var_name, &val) != NIMCP_OK) {
                    val.type = NIMCP_POLICY_VALUE_NULL;
                }
                stack_push(stack, val);
                break;
            }

            case OP_LOAD_MEMBER: {
                // Simplified: just look up member as variable
                nimcp_policy_value_t val = {0};
                const char* member = bc->string_pool[instr.index];
                if (nimcp_policy_context_get(ctx, member, &val) != NIMCP_OK) {
                    val.type = NIMCP_POLICY_VALUE_NULL;
                }
                stack_pop(stack); // Pop object
                stack_push(stack, val);
                break;
            }

            case OP_CALL: {
                const char* func_name = bc->string_pool[instr.index];
                void* user_data = NULL;
                nimcp_policy_function_t func = registry_lookup(registry, func_name, &user_data);

                if (!func) {
                    LOG_ERROR("Unknown function: %s", func_name);
                    error = NIMCP_ERROR_INVALID_PARAM;
                    goto cleanup;
                }

                // Get arguments from stack (simplified - assuming 2 args max)
                nimcp_policy_value_t args[2] = {0};
                size_t num_args = 0;
                if (stack->count >= 2) {
                    args[1] = stack_pop(stack);
                    args[0] = stack_pop(stack);
                    num_args = 2;
                } else if (stack->count >= 1) {
                    args[0] = stack_pop(stack);
                    num_args = 1;
                }

                nimcp_policy_value_t func_result = {0};
                error = func(args, num_args, &func_result, user_data);

                if (error != NIMCP_OK) {
                    goto cleanup;
                }

                stack_push(stack, func_result);
                break;
            }

            case OP_ADD:
            case OP_SUB:
            case OP_MUL:
            case OP_DIV:
            case OP_MOD: {
                nimcp_policy_value_t right = stack_pop(stack);
                nimcp_policy_value_t left = stack_pop(stack);
                nimcp_policy_value_t result_val = {0};

                if (left.type == NIMCP_POLICY_VALUE_INT && right.type == NIMCP_POLICY_VALUE_INT) {
                    result_val.type = NIMCP_POLICY_VALUE_INT;
                    switch (instr.opcode) {
                        case OP_ADD: result_val.int_val = left.int_val + right.int_val; break;
                        case OP_SUB: result_val.int_val = left.int_val - right.int_val; break;
                        case OP_MUL: result_val.int_val = left.int_val * right.int_val; break;
                        case OP_DIV: result_val.int_val = right.int_val ? left.int_val / right.int_val : 0; break;
                        case OP_MOD: result_val.int_val = right.int_val ? left.int_val % right.int_val : 0; break;
                        default: break;
                    }
                } else {
                    double l = left.type == NIMCP_POLICY_VALUE_FLOAT ? left.float_val : left.int_val;
                    double r = right.type == NIMCP_POLICY_VALUE_FLOAT ? right.float_val : right.int_val;
                    result_val.type = NIMCP_POLICY_VALUE_FLOAT;
                    switch (instr.opcode) {
                        case OP_ADD: result_val.float_val = l + r; break;
                        case OP_SUB: result_val.float_val = l - r; break;
                        case OP_MUL: result_val.float_val = l * r; break;
                        case OP_DIV: result_val.float_val = r != 0 ? l / r : 0; break;
                        case OP_MOD: result_val.float_val = fmod(l, r); break;
                        default: break;
                    }
                }
                stack_push(stack, result_val);
                break;
            }

            case OP_EQ:
            case OP_NE:
            case OP_LT:
            case OP_LE:
            case OP_GT:
            case OP_GE: {
                nimcp_policy_value_t right = stack_pop(stack);
                nimcp_policy_value_t left = stack_pop(stack);
                nimcp_policy_value_t result_val = {0};
                result_val.type = NIMCP_POLICY_VALUE_BOOL;

                if (left.type == NIMCP_POLICY_VALUE_STRING && right.type == NIMCP_POLICY_VALUE_STRING) {
                    int cmp = strcmp(left.string_val, right.string_val);
                    switch (instr.opcode) {
                        case OP_EQ: result_val.bool_val = (cmp == 0); break;
                        case OP_NE: result_val.bool_val = (cmp != 0); break;
                        case OP_LT: result_val.bool_val = (cmp < 0); break;
                        case OP_LE: result_val.bool_val = (cmp <= 0); break;
                        case OP_GT: result_val.bool_val = (cmp > 0); break;
                        case OP_GE: result_val.bool_val = (cmp >= 0); break;
                        default: break;
                    }
                } else {
                    double l = left.type == NIMCP_POLICY_VALUE_FLOAT ? left.float_val : left.int_val;
                    double r = right.type == NIMCP_POLICY_VALUE_FLOAT ? right.float_val : right.int_val;
                    switch (instr.opcode) {
                        case OP_EQ: result_val.bool_val = (l == r); break;
                        case OP_NE: result_val.bool_val = (l != r); break;
                        case OP_LT: result_val.bool_val = (l < r); break;
                        case OP_LE: result_val.bool_val = (l <= r); break;
                        case OP_GT: result_val.bool_val = (l > r); break;
                        case OP_GE: result_val.bool_val = (l >= r); break;
                        default: break;
                    }
                }
                stack_push(stack, result_val);
                break;
            }

            case OP_AND: {
                nimcp_policy_value_t right = stack_pop(stack);
                nimcp_policy_value_t left = stack_pop(stack);
                nimcp_policy_value_t result_val = {0};
                result_val.type = NIMCP_POLICY_VALUE_BOOL;
                result_val.bool_val = value_to_bool(&left) && value_to_bool(&right);
                stack_push(stack, result_val);
                break;
            }

            case OP_OR: {
                nimcp_policy_value_t right = stack_pop(stack);
                nimcp_policy_value_t left = stack_pop(stack);
                nimcp_policy_value_t result_val = {0};
                result_val.type = NIMCP_POLICY_VALUE_BOOL;
                result_val.bool_val = value_to_bool(&left) || value_to_bool(&right);
                stack_push(stack, result_val);
                break;
            }

            case OP_NOT: {
                nimcp_policy_value_t val = stack_pop(stack);
                nimcp_policy_value_t result_val = {0};
                result_val.type = NIMCP_POLICY_VALUE_BOOL;
                result_val.bool_val = !value_to_bool(&val);
                stack_push(stack, result_val);
                break;
            }

            case OP_NEG: {
                nimcp_policy_value_t val = stack_pop(stack);
                if (val.type == NIMCP_POLICY_VALUE_INT) {
                    val.int_val = -val.int_val;
                } else if (val.type == NIMCP_POLICY_VALUE_FLOAT) {
                    val.float_val = -val.float_val;
                }
                stack_push(stack, val);
                break;
            }

            case OP_ACTION: {
                const char* action_type = bc->string_pool[instr.index];
                result->message = strdup("");
                result->rule_name = strdup("");
                if (!result->message || !result->rule_name) {
                    nimcp_free(result->message);
                    nimcp_free(result->rule_name);
                    result->message = NULL;
                    result->rule_name = NULL;
                    error = NIMCP_ERROR_NO_MEMORY;
                    goto cleanup;
                }

                if (strcmp(action_type, "ALLOW") == 0) {
                    result->action = NIMCP_POLICY_ACTION_ALLOW;
                } else if (strcmp(action_type, "DENY") == 0) {
                    result->action = NIMCP_POLICY_ACTION_DENY;
                } else if (strcmp(action_type, "THROTTLE") == 0) {
                    result->action = NIMCP_POLICY_ACTION_THROTTLE;
                } else {
                    result->action = NIMCP_POLICY_ACTION_CUSTOM;
                }
                break;
            }

            case OP_RETURN:
                goto done;

            default:
                LOG_ERROR("Unknown opcode: %d", instr.opcode);
                error = NIMCP_ERROR_INVALID_PARAM;
                goto cleanup;
        }
    }

done:
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    result->eval_time_ns = (end_time.tv_sec - start_time.tv_sec) * 1000000000ULL +
                          (end_time.tv_nsec - start_time.tv_nsec);

cleanup:
    stack_destroy(stack);
    return error;
}

void nimcp_policy_result_free(nimcp_policy_result_t* result) {
    if (!result) return;

    nimcp_free(result->message);
    nimcp_free(result->rule_name);
    nimcp_free(result->params);

    memset(result, 0, sizeof(nimcp_policy_result_t));
}
