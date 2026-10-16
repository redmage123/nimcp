/**
 * @file nimcp_policy_compiler.c
 * @brief NIMCP Policy Compiler - AST to Bytecode
 *
 * WHAT: Compiles AST into bytecode for efficient evaluation.
 * WHY:  Bytecode interpretation is faster than AST walking and enables
 *       optimization passes.
 * HOW:  Traverses AST and emits stack-based bytecode instructions.
 */

#include "security/nimcp_policy_ast.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "security/nimcp_policy_engine.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stddef.h>  /* for NULL */
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(policy_compiler, MESH_ADAPTER_CATEGORY_SECURITY)


/* ========================================================================
 * Bytecode Instructions
 * ======================================================================== */

typedef enum {
    OP_PUSH_INT,
    OP_PUSH_FLOAT,
    OP_PUSH_STRING,
    OP_PUSH_BOOL,
    OP_LOAD_VAR,
    OP_LOAD_MEMBER,
    OP_CALL,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_AND,
    OP_OR,
    OP_NOT,
    OP_NEG,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_RETURN,
    OP_ACTION
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

/* ========================================================================
 * Bytecode Builder
 * ======================================================================== */

static bytecode_t* bytecode_create(void) {
    bytecode_t* bc = nimcp_calloc(1, sizeof(bytecode_t));
    if (!bc) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bc is NULL");

        return NULL;

    }

    bc->capacity = 64;
    bc->instructions = nimcp_calloc(bc->capacity, sizeof(instruction_t));
    if (!bc->instructions) {
        nimcp_free(bc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "bytecode_create: bc->instructions is NULL");
        return NULL;
    }

    return bc;
}

static void bytecode_destroy(bytecode_t* bc) {
    if (!bc) return;

    for (size_t i = 0; i < bc->string_count; i++) {
        nimcp_free(bc->string_pool[i]);
    }
    nimcp_free(bc->string_pool);
    nimcp_free(bc->instructions);
    nimcp_free(bc);
}

static void emit(bytecode_t* bc, instruction_t instr) {
    if (bc->count >= bc->capacity) {
        size_t new_capacity = bc->capacity * 2;
        // Guard against integer overflow
        if (new_capacity < bc->capacity || new_capacity > SIZE_MAX / sizeof(instruction_t)) {
            return;  // Overflow, cannot grow
        }
        instruction_t* new_instructions = nimcp_realloc(bc->instructions,
                                                   new_capacity * sizeof(instruction_t));
        if (!new_instructions) {
            return;  // Keep original buffer intact
        }
        bc->instructions = new_instructions;
        bc->capacity = new_capacity;
    }
    bc->instructions[bc->count++] = instr;
}

static size_t add_string(bytecode_t* bc, const char* str) {
    // Check if string already exists
    for (size_t i = 0; i < bc->string_count; i++) {
        if (strcmp(bc->string_pool[i], str) == 0) {
            return i;
        }
    }

    // Add new string
    char** new_pool = nimcp_realloc(bc->string_pool,
                              (bc->string_count + 1) * sizeof(char*));
    if (!new_pool) {
        return (size_t)-1;  // Error indicator
    }
    bc->string_pool = new_pool;

    char* dup = strdup(str);
    if (!dup) {
        return (size_t)-1;  // Error indicator
    }
    bc->string_pool[bc->string_count] = dup;
    return bc->string_count++;
}

/* ========================================================================
 * Compiler
 * ======================================================================== */

static bool compile_node(bytecode_t* bc, const nimcp_ast_node_t* node);

static bool compile_binary(bytecode_t* bc, const nimcp_ast_node_t* node) {
    // Compile left operand
    if (!compile_node(bc, node->binary.left)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_binary: compile_node is NULL");
        return false;
    }

    // Compile right operand
    if (!compile_node(bc, node->binary.right)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_binary: compile_node is NULL");
        return false;
    }

    // Emit operator
    instruction_t instr = {0};
    switch (node->binary.op) {
        case NIMCP_OP_ADD: instr.opcode = OP_ADD; break;
        case NIMCP_OP_SUB: instr.opcode = OP_SUB; break;
        case NIMCP_OP_MUL: instr.opcode = OP_MUL; break;
        case NIMCP_OP_DIV: instr.opcode = OP_DIV; break;
        case NIMCP_OP_MOD: instr.opcode = OP_MOD; break;
        case NIMCP_OP_EQ: instr.opcode = OP_EQ; break;
        case NIMCP_OP_NE: instr.opcode = OP_NE; break;
        case NIMCP_OP_LT: instr.opcode = OP_LT; break;
        case NIMCP_OP_LE: instr.opcode = OP_LE; break;
        case NIMCP_OP_GT: instr.opcode = OP_GT; break;
        case NIMCP_OP_GE: instr.opcode = OP_GE; break;
        case NIMCP_OP_AND: instr.opcode = OP_AND; break;
        case NIMCP_OP_OR: instr.opcode = OP_OR; break;
        default:
            LOG_ERROR("Unknown binary operator: %d", node->binary.op);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_binary: operation failed");
            return false;
    }
    emit(bc, instr);

    return true;
}

static bool compile_unary(bytecode_t* bc, const nimcp_ast_node_t* node) {
    if (!compile_node(bc, node->unary.operand)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_unary: compile_node is NULL");
        return false;
    }

    instruction_t instr = {0};
    switch (node->unary.op) {
        case NIMCP_OP_NOT: instr.opcode = OP_NOT; break;
        case NIMCP_OP_NEG: instr.opcode = OP_NEG; break;
        default:
            LOG_ERROR("Unknown unary operator: %d", node->unary.op);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_unary: operation failed");
            return false;
    }
    emit(bc, instr);

    return true;
}

static bool compile_call(bytecode_t* bc, const nimcp_ast_node_t* node) {
    // Compile arguments in order
    for (size_t i = 0; i < node->call.num_args; i++) {
        if (!compile_node(bc, node->call.args[i])) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_call: compile_node is NULL");
            return false;
        }
    }

    // Emit call instruction
    instruction_t instr = {0};
    instr.opcode = OP_CALL;
    instr.index = add_string(bc, node->call.name);
    emit(bc, instr);

    return true;
}

static bool compile_member(bytecode_t* bc, const nimcp_ast_node_t* node) {
    // Compile object
    if (!compile_node(bc, node->member.object)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_member: compile_node is NULL");
        return false;
    }

    // Emit member access
    instruction_t instr = {0};
    instr.opcode = OP_LOAD_MEMBER;
    instr.index = add_string(bc, node->member.member);
    emit(bc, instr);

    return true;
}

static bool compile_node(bytecode_t* bc, const nimcp_ast_node_t* node) {
    if (!node) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "compile_node: node is NULL");
        return false;
    }

    instruction_t instr = {0};

    switch (node->type) {
        case NIMCP_AST_LITERAL:
            switch (node->literal.type) {
                case NIMCP_LITERAL_STRING:
                    instr.opcode = OP_PUSH_STRING;
                    instr.index = add_string(bc, node->literal.string_val);
                    emit(bc, instr);
                    break;
                case NIMCP_LITERAL_INT:
                    instr.opcode = OP_PUSH_INT;
                    instr.int_val = node->literal.int_val;
                    emit(bc, instr);
                    break;
                case NIMCP_LITERAL_FLOAT:
                    instr.opcode = OP_PUSH_FLOAT;
                    instr.float_val = node->literal.float_val;
                    emit(bc, instr);
                    break;
                case NIMCP_LITERAL_BOOL:
                    instr.opcode = OP_PUSH_BOOL;
                    instr.bool_val = node->literal.bool_val;
                    emit(bc, instr);
                    break;
            }
            break;

        case NIMCP_AST_IDENTIFIER:
            instr.opcode = OP_LOAD_VAR;
            instr.index = add_string(bc, node->identifier.name);
            emit(bc, instr);
            break;

        case NIMCP_AST_BINARY_OP:
            return compile_binary(bc, node);

        case NIMCP_AST_UNARY_OP:
            return compile_unary(bc, node);

        case NIMCP_AST_CALL:
            return compile_call(bc, node);

        case NIMCP_AST_MEMBER:
            return compile_member(bc, node);

        case NIMCP_AST_ACTION:
            instr.opcode = OP_ACTION;
            instr.index = add_string(bc, node->action.action_type);
            emit(bc, instr);
            break;

        default:
            LOG_WARN("Compilation not implemented for node type %d", node->type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "compile_node: operation failed");
            return false;
    }

    return true;
}

bytecode_t* nimcp_policy_compile(const nimcp_ast_node_t* ast) {
    LOG_INFO("Compiling policy AST to bytecode");

    bytecode_t* bc = bytecode_create();
    if (!bc) {
        LOG_ERROR("Failed to create bytecode");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_policy_compile: bc is NULL");
        return NULL;
    }

    if (!compile_node(bc, ast)) {
        LOG_ERROR("Compilation failed");
        bytecode_destroy(bc);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_policy_compile: compile_node is NULL");
        return NULL;
    }

    // Emit return
    instruction_t ret = {0};
    ret.opcode = OP_RETURN;
    emit(bc, ret);

    LOG_INFO("Successfully compiled policy (%zu instructions)", bc->count);
    return bc;
}

void nimcp_policy_bytecode_destroy(bytecode_t* bc) {
    bytecode_destroy(bc);
}
