/**
 * @file nimcp_fix_templates.h
 * @brief Fix Template Library for Code Generation
 *
 * WHAT: Collection of code fix templates for common error types
 * WHY:  Standardized, proven fix patterns for autonomous repair
 * HOW:  Template strings with placeholders for context-specific values
 *
 * TEMPLATE SYNTAX:
 * - ${VAR_NAME} - Variable placeholder
 * - ${ORIGINAL_CODE} - Original problematic code
 * - ${CONDITION} - Generated condition expression
 * - ${ERROR_VALUE} - Safe error return value
 * - ${TYPE} - Variable/parameter type
 *
 * @author NIMCP Development Team
 * @date 2025-01-20
 * @version 1.0.0
 */

#ifndef NIMCP_FIX_TEMPLATES_H
#define NIMCP_FIX_TEMPLATES_H

#include <stdint.h>
#include <stdbool.h>
#include "cognitive/parietal/nimcp_code_generation.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define FIX_TEMPLATE_MAX_CODE       2048    /**< Max template code size */
#define FIX_TEMPLATE_MAX_DESC       256     /**< Max description size */
#define FIX_TEMPLATE_MAX_VARS       16      /**< Max variables per template */
#define FIX_TEMPLATE_VAR_NAME_MAX   64      /**< Max variable name length */

//=============================================================================
// Template Variable Types
//=============================================================================

/**
 * @brief Template variable type for placeholder substitution
 */
typedef enum {
    TEMPLATE_VAR_STRING = 0,        /**< String substitution */
    TEMPLATE_VAR_INTEGER,           /**< Integer substitution */
    TEMPLATE_VAR_FLOAT,             /**< Float substitution */
    TEMPLATE_VAR_TYPE_NAME,         /**< C type name */
    TEMPLATE_VAR_IDENTIFIER,        /**< C identifier (var/func name) */
    TEMPLATE_VAR_EXPRESSION,        /**< C expression */
    TEMPLATE_VAR_CODE_BLOCK         /**< Multi-line code block */
} template_var_type_t;

/**
 * @brief Template variable definition
 */
typedef struct {
    char name[FIX_TEMPLATE_VAR_NAME_MAX];   /**< Variable name (e.g., "VAR_NAME") */
    template_var_type_t type;               /**< Variable type */
    bool required;                          /**< Is variable required */
    char default_value[128];                /**< Default value if optional */
    char description[128];                  /**< Variable description */
} template_variable_t;

/**
 * @brief Fix template definition
 */
typedef struct {
    uint32_t template_id;                   /**< Unique template ID */
    code_fix_strategy_t strategy;           /**< Strategy this implements */
    fix_complexity_t complexity;            /**< Expected complexity */

    char name[64];                          /**< Template name */
    char description[FIX_TEMPLATE_MAX_DESC]; /**< Human-readable description */
    char code[FIX_TEMPLATE_MAX_CODE];       /**< Template code with placeholders */

    template_variable_t variables[FIX_TEMPLATE_MAX_VARS]; /**< Variable definitions */
    uint32_t variable_count;                /**< Number of variables */

    float base_confidence;                  /**< Base confidence score */
    float base_risk;                        /**< Base risk score */

    error_type_t applicable_errors[8];      /**< Error types this can fix */
    uint32_t applicable_error_count;        /**< Number of applicable errors */
} fix_template_t;

/**
 * @brief Template instantiation context
 */
typedef struct {
    const fix_template_t* template_def;     /**< Template definition */
    char var_values[FIX_TEMPLATE_MAX_VARS][512]; /**< Variable values */
    bool var_set[FIX_TEMPLATE_MAX_VARS];    /**< Which variables are set */
} template_context_t;

//=============================================================================
// Built-in Templates - NULL Check
//=============================================================================

/**
 * @brief Null pointer check template - simple return
 *
 * Adds null check that returns error value on null pointer.
 */
#define FIX_TEMPLATE_NULL_CHECK_RETURN \
    "if (${VAR_NAME} == NULL) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Null pointer check template - with logging
 */
#define FIX_TEMPLATE_NULL_CHECK_LOG \
    "if (${VAR_NAME} == NULL) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: ${VAR_NAME} is NULL\");\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Null pointer check template - with default value
 */
#define FIX_TEMPLATE_NULL_CHECK_DEFAULT \
    "if (${VAR_NAME} == NULL) {\n" \
    "    ${VAR_NAME} = ${DEFAULT_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

//=============================================================================
// Built-in Templates - Bounds Check
//=============================================================================

/**
 * @brief Array bounds check template - clamp to bounds
 */
#define FIX_TEMPLATE_BOUNDS_CHECK_CLAMP \
    "if (${INDEX_VAR} < 0) {\n" \
    "    ${INDEX_VAR} = 0;\n" \
    "} else if (${INDEX_VAR} >= ${ARRAY_SIZE}) {\n" \
    "    ${INDEX_VAR} = ${ARRAY_SIZE} - 1;\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Array bounds check template - return on out of bounds
 */
#define FIX_TEMPLATE_BOUNDS_CHECK_RETURN \
    "if (${INDEX_VAR} < 0 || ${INDEX_VAR} >= ${ARRAY_SIZE}) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Array bounds check template - with size parameter
 */
#define FIX_TEMPLATE_BOUNDS_CHECK_SIZE_PARAM \
    "if (${INDEX_VAR} >= ${SIZE_PARAM}) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: index %zu out of bounds (max %zu)\", " \
    "(size_t)${INDEX_VAR}, (size_t)${SIZE_PARAM});\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

//=============================================================================
// Built-in Templates - Division Guard
//=============================================================================

/**
 * @brief Division by zero check template - return on zero
 */
#define FIX_TEMPLATE_DIVISION_GUARD_RETURN \
    "if (${DIVISOR} == 0) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Division by zero check template - use epsilon
 */
#define FIX_TEMPLATE_DIVISION_GUARD_EPSILON \
    "if (fabs(${DIVISOR}) < ${EPSILON}) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Division by zero check template - substitute safe value
 */
#define FIX_TEMPLATE_DIVISION_GUARD_SAFE \
    "${TYPE} ${RESULT_VAR};\n" \
    "if (${DIVISOR} == 0) {\n" \
    "    ${RESULT_VAR} = ${SAFE_VALUE};\n" \
    "} else {\n" \
    "    ${RESULT_VAR} = ${DIVIDEND} / ${DIVISOR};\n" \
    "}"

//=============================================================================
// Built-in Templates - NaN/Inf Guard
//=============================================================================

/**
 * @brief NaN check template - return on NaN
 */
#define FIX_TEMPLATE_NAN_CHECK_RETURN \
    "if (isnan(${VAR_NAME}) || isinf(${VAR_NAME})) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief NaN check template - substitute safe value
 */
#define FIX_TEMPLATE_NAN_CHECK_SUBSTITUTE \
    "if (isnan(${VAR_NAME})) {\n" \
    "    ${VAR_NAME} = ${SAFE_VALUE};\n" \
    "} else if (isinf(${VAR_NAME})) {\n" \
    "    ${VAR_NAME} = ${VAR_NAME} > 0 ? ${MAX_VALUE} : ${MIN_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief NaN propagation guard - check result
 */
#define FIX_TEMPLATE_NAN_RESULT_CHECK \
    "${ORIGINAL_CODE}\n" \
    "if (isnan(${RESULT_VAR}) || isinf(${RESULT_VAR})) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: numerical instability detected\");\n" \
    "    return ${ERROR_VALUE};\n" \
    "}"

//=============================================================================
// Built-in Templates - Memory Cleanup
//=============================================================================

/**
 * @brief Memory leak fix - add free before return
 */
#define FIX_TEMPLATE_MEMORY_FREE_RETURN \
    "if (${PTR_VAR} != NULL) {\n" \
    "    nimcp_free(${PTR_VAR});\n" \
    "    ${PTR_VAR} = NULL;\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Memory leak fix - add cleanup label pattern
 */
#define FIX_TEMPLATE_MEMORY_CLEANUP_GOTO \
    "${ORIGINAL_CODE}\n" \
    "    goto cleanup;\n" \
    "\n" \
    "cleanup:\n" \
    "    if (${PTR_VAR} != NULL) {\n" \
    "        nimcp_free(${PTR_VAR});\n" \
    "    }\n" \
    "    return ${RETURN_VALUE};"

/**
 * @brief Double free prevention - null after free
 */
#define FIX_TEMPLATE_DOUBLE_FREE_GUARD \
    "if (${PTR_VAR} != NULL) {\n" \
    "    nimcp_free(${PTR_VAR});\n" \
    "    ${PTR_VAR} = NULL;\n" \
    "}"

//=============================================================================
// Built-in Templates - Mutex/Lock
//=============================================================================

/**
 * @brief Mutex lock with error check
 */
#define FIX_TEMPLATE_MUTEX_LOCK_CHECK \
    "int ${LOCK_RESULT} = nimcp_mutex_lock(${MUTEX_VAR});\n" \
    "if (${LOCK_RESULT} != 0) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: mutex lock failed: %d\", ${LOCK_RESULT});\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Ensure mutex unlock on all paths
 */
#define FIX_TEMPLATE_MUTEX_UNLOCK_ENSURE \
    "nimcp_mutex_lock(${MUTEX_VAR});\n" \
    "do {\n" \
    "    ${ORIGINAL_CODE}\n" \
    "} while (0);\n" \
    "nimcp_mutex_unlock(${MUTEX_VAR});"

/**
 * @brief Deadlock prevention - trylock with timeout
 */
#define FIX_TEMPLATE_MUTEX_TRYLOCK \
    "if (nimcp_mutex_trylock(${MUTEX_VAR}) != 0) {\n" \
    "    nimcp_log_warning(\"${FUNC_NAME}: mutex contention, retrying\");\n" \
    "    if (nimcp_mutex_timedlock(${MUTEX_VAR}, ${TIMEOUT_MS}) != 0) {\n" \
    "        return ${ERROR_VALUE};\n" \
    "    }\n" \
    "}\n" \
    "${ORIGINAL_CODE}\n" \
    "nimcp_mutex_unlock(${MUTEX_VAR});"

//=============================================================================
// Built-in Templates - Initialization
//=============================================================================

/**
 * @brief Variable initialization template
 */
#define FIX_TEMPLATE_VAR_INIT \
    "${TYPE} ${VAR_NAME} = ${INIT_VALUE};\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Struct initialization template
 */
#define FIX_TEMPLATE_STRUCT_INIT \
    "memset(&${VAR_NAME}, 0, sizeof(${VAR_NAME}));\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Array initialization template
 */
#define FIX_TEMPLATE_ARRAY_INIT \
    "memset(${VAR_NAME}, 0, sizeof(${VAR_NAME}));\n" \
    "${ORIGINAL_CODE}"

//=============================================================================
// Built-in Templates - Overflow Guard
//=============================================================================

/**
 * @brief Integer overflow check - addition
 */
#define FIX_TEMPLATE_OVERFLOW_ADD \
    "if (${VAR_A} > 0 && ${VAR_B} > ${TYPE_MAX} - ${VAR_A}) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: integer overflow in addition\");\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Integer overflow check - multiplication
 */
#define FIX_TEMPLATE_OVERFLOW_MUL \
    "if (${VAR_A} != 0 && ${VAR_B} > ${TYPE_MAX} / ${VAR_A}) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: integer overflow in multiplication\");\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Size overflow check for allocation
 */
#define FIX_TEMPLATE_SIZE_OVERFLOW \
    "if (${COUNT} > SIZE_MAX / sizeof(${ELEMENT_TYPE})) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: size overflow in allocation\");\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

//=============================================================================
// Built-in Templates - Error Handling
//=============================================================================

/**
 * @brief Add return value check
 */
#define FIX_TEMPLATE_RETVAL_CHECK \
    "${TYPE} ${RESULT_VAR} = ${ORIGINAL_CODE};\n" \
    "if (${RESULT_VAR} ${ERROR_CONDITION}) {\n" \
    "    nimcp_log_error(\"${FUNC_NAME}: ${CALLED_FUNC} failed: %d\", (int)${RESULT_VAR});\n" \
    "    return ${ERROR_VALUE};\n" \
    "}"

/**
 * @brief Add try-finally pattern (using goto)
 */
#define FIX_TEMPLATE_TRY_FINALLY \
    "${RESOURCE_TYPE} ${RESOURCE_VAR} = ${RESOURCE_ALLOC};\n" \
    "if (${RESOURCE_VAR} == NULL) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "\n" \
    "int ${RESULT_VAR} = 0;\n" \
    "${ORIGINAL_CODE}\n" \
    "\n" \
    "finally:\n" \
    "    ${RESOURCE_FREE}(${RESOURCE_VAR});\n" \
    "    return ${RESULT_VAR};"

//=============================================================================
// Built-in Templates - Alignment
//=============================================================================

/**
 * @brief Ensure alignment for pointer
 */
#define FIX_TEMPLATE_ALIGN_POINTER \
    "${TYPE}* ${ALIGNED_VAR} = (${TYPE}*)(((uintptr_t)${PTR_VAR} + ${ALIGNMENT} - 1) & ~(${ALIGNMENT} - 1));\n" \
    "${ORIGINAL_CODE}"

/**
 * @brief Aligned allocation
 */
#define FIX_TEMPLATE_ALIGNED_ALLOC \
    "void* ${PTR_VAR} = nimcp_aligned_alloc(${ALIGNMENT}, ${SIZE});\n" \
    "if (${PTR_VAR} == NULL) {\n" \
    "    return ${ERROR_VALUE};\n" \
    "}\n" \
    "${ORIGINAL_CODE}"

//=============================================================================
// Template Registry Functions
//=============================================================================

/**
 * @brief Get built-in template by strategy
 *
 * @param strategy Fix strategy
 * @param variant Template variant (0 for default)
 * @return Template definition or NULL if not found
 */
const fix_template_t* fix_template_get_builtin(
    code_fix_strategy_t strategy,
    uint32_t variant
);

/**
 * @brief Get number of built-in templates for strategy
 *
 * @param strategy Fix strategy
 * @return Number of available templates
 */
uint32_t fix_template_count_for_strategy(code_fix_strategy_t strategy);

/**
 * @brief Create template context for instantiation
 *
 * @param template_def Template definition
 * @return Context or NULL on error
 */
template_context_t* fix_template_create_context(const fix_template_t* template_def);

/**
 * @brief Destroy template context
 *
 * @param ctx Context to destroy
 */
void fix_template_destroy_context(template_context_t* ctx);

/**
 * @brief Set template variable value
 *
 * @param ctx Template context
 * @param var_name Variable name
 * @param value Value to set
 * @return 0 on success, -1 if variable not found
 */
int fix_template_set_variable(
    template_context_t* ctx,
    const char* var_name,
    const char* value
);

/**
 * @brief Instantiate template with context
 *
 * @param ctx Template context with variables set
 * @param output Output buffer for instantiated code
 * @param output_size Output buffer size
 * @return 0 on success, -1 on error
 */
int fix_template_instantiate(
    const template_context_t* ctx,
    char* output,
    size_t output_size
);

/**
 * @brief Validate template context (all required variables set)
 *
 * @param ctx Template context
 * @return true if valid
 */
bool fix_template_validate_context(const template_context_t* ctx);

/**
 * @brief Get template variable definition
 *
 * @param template_def Template definition
 * @param var_name Variable name
 * @return Variable definition or NULL if not found
 */
const template_variable_t* fix_template_get_variable(
    const fix_template_t* template_def,
    const char* var_name
);

/**
 * @brief Parse template to extract variable names
 *
 * @param template_code Template code string
 * @param variables Output variable names array
 * @param max_variables Max variables to extract
 * @return Number of variables found
 */
int fix_template_parse_variables(
    const char* template_code,
    char variables[][FIX_TEMPLATE_VAR_NAME_MAX],
    uint32_t max_variables
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FIX_TEMPLATES_H */
