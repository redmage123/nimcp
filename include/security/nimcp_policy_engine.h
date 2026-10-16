/**
 * @file nimcp_policy_engine.h
 * @brief NIMCP Security Policy Engine
 *
 * WHAT: Declarative security policy language engine for NIMCP. Provides
 *       parsing, compilation, and evaluation of security policies written
 *       in NIMCP Security Policy Language (NSPL).
 *
 * WHY:  Security policies need to be maintainable, auditable, and modifiable
 *       without recompilation. A declarative language allows security teams
 *       to define rules independently of application code.
 *
 * HOW:  Implements a complete policy pipeline: text -> tokens -> AST ->
 *       bytecode -> evaluation. Integrates with bio-async for event
 *       processing and supports hot-reload for dynamic updates.
 */

#ifndef NIMCP_POLICY_ENGINE_H
#define NIMCP_POLICY_ENGINE_H

#include "security/nimcp_policy_ast.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic number for policy engine validation */
#define NIMCP_POLICY_ENGINE_MAGIC 0x504F4C45  /* 'POLE' */

/* Opaque types */
typedef struct nimcp_policy_engine* nimcp_policy_engine_t;
typedef struct nimcp_policy* nimcp_policy_t;
typedef struct nimcp_policy_context* nimcp_policy_context_t;

/**
 * Policy action types
 */
typedef enum {
    NIMCP_POLICY_ACTION_ALLOW,
    NIMCP_POLICY_ACTION_DENY,
    NIMCP_POLICY_ACTION_THROTTLE,
    NIMCP_POLICY_ACTION_LOG,
    NIMCP_POLICY_ACTION_ALERT,
    NIMCP_POLICY_ACTION_CUSTOM
} nimcp_policy_action_t;

/**
 * Policy severity levels
 */
typedef enum {
    NIMCP_POLICY_SEVERITY_INFO,
    NIMCP_POLICY_SEVERITY_LOW,
    NIMCP_POLICY_SEVERITY_MEDIUM,
    NIMCP_POLICY_SEVERITY_HIGH,
    NIMCP_POLICY_SEVERITY_CRITICAL
} nimcp_policy_severity_t;

/**
 * Policy value types
 */
typedef enum {
    NIMCP_POLICY_VALUE_NULL,
    NIMCP_POLICY_VALUE_STRING,
    NIMCP_POLICY_VALUE_INT,
    NIMCP_POLICY_VALUE_FLOAT,
    NIMCP_POLICY_VALUE_BOOL
} nimcp_policy_value_type_t;

/**
 * Policy value
 */
typedef struct {
    nimcp_policy_value_type_t type;
    union {
        char* string_val;
        int64_t int_val;
        double float_val;
        bool bool_val;
    };
} nimcp_policy_value_t;

/**
 * Policy evaluation result
 */
typedef struct {
    nimcp_policy_action_t action;
    nimcp_policy_severity_t severity;
    char* message;
    char* rule_name;
    nimcp_policy_value_t* params;
    size_t num_params;
    bool should_log;
    uint64_t eval_time_ns;
} nimcp_policy_result_t;

/**
 * Policy engine configuration
 */
typedef struct {
    size_t max_policies;
    size_t max_rules_per_policy;
    bool enable_caching;
    size_t cache_size;
    bool enable_optimization;
    bool enable_hot_reload;
    const char* policy_directory;
    bio_router_t bio_router;  /* Optional bio-async integration */
} nimcp_policy_engine_config_t;

/**
 * Policy engine statistics
 */
typedef struct {
    size_t num_policies;
    size_t num_rules;
    uint64_t total_evaluations;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint64_t avg_eval_time_ns;
    uint64_t max_eval_time_ns;
    size_t parse_errors;
    size_t eval_errors;
} nimcp_policy_stats_t;

/**
 * Policy function callback
 */
typedef nimcp_error_t (*nimcp_policy_function_t)(
    const nimcp_policy_value_t* args,
    size_t num_args,
    nimcp_policy_value_t* result,
    void* user_data
);

/**
 * Policy event callback
 */
typedef void (*nimcp_policy_event_callback_t)(
    const char* event_type,
    const nimcp_policy_result_t* result,
    void* user_data
);

/* ========================================================================
 * Engine Lifecycle
 * ======================================================================== */

/**
 * Create policy engine
 *
 * WHAT: Creates a new policy engine instance with the given configuration.
 * WHY:  Initializes all internal structures needed for parsing, compiling,
 *       and evaluating policies.
 * HOW:  Allocates memory, initializes parser and compiler, sets up cache.
 *
 * @param config Configuration settings
 * @return Policy engine instance or NULL on error
 */
nimcp_policy_engine_t nimcp_policy_engine_create(
    const nimcp_policy_engine_config_t* config
);

/**
 * Destroy policy engine
 *
 * WHAT: Destroys the policy engine and frees all resources.
 * WHY:  Prevents memory leaks and ensures clean shutdown.
 * HOW:  Unloads all policies, frees cache, destroys parser/compiler.
 *
 * @param engine Policy engine instance
 */
void nimcp_policy_engine_destroy(nimcp_policy_engine_t engine);

/* ========================================================================
 * Policy Management
 * ======================================================================== */

/**
 * Load policy from text
 *
 * WHAT: Parses and compiles a policy from text string.
 * WHY:  Allows dynamic policy loading at runtime.
 * HOW:  Tokenizes, parses to AST, compiles to bytecode, stores in engine.
 *
 * @param engine Policy engine instance
 * @param policy_text Policy source text
 * @param policy Output policy handle
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_policy_engine_load(
    nimcp_policy_engine_t engine,
    const char* policy_text,
    nimcp_policy_t* policy
);

/**
 * Load policy from file
 *
 * WHAT: Loads and compiles a policy from a file.
 * WHY:  Supports file-based policy management.
 * HOW:  Reads file, calls nimcp_policy_engine_load().
 *
 * @param engine Policy engine instance
 * @param filepath Path to policy file
 * @param policy Output policy handle
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_policy_engine_load_file(
    nimcp_policy_engine_t engine,
    const char* filepath,
    nimcp_policy_t* policy
);

/**
 * Unload policy
 *
 * WHAT: Removes a policy from the engine.
 * WHY:  Allows dynamic policy updates.
 * HOW:  Removes from policy list, frees bytecode and AST.
 *
 * @param engine Policy engine instance
 * @param policy Policy handle
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_policy_engine_unload(
    nimcp_policy_engine_t engine,
    nimcp_policy_t policy
);

/**
 * Reload all policies
 *
 * WHAT: Reloads all policies from their source files.
 * WHY:  Supports hot-reload for policy updates.
 * HOW:  Re-parses and re-compiles all policies.
 *
 * @param engine Policy engine instance
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_policy_engine_reload(nimcp_policy_engine_t engine);

/* ========================================================================
 * Context Management
 * ======================================================================== */

/**
 * Create evaluation context
 *
 * WHAT: Creates a context for policy evaluation.
 * WHY:  Provides binding environment for variables (input, request, etc.).
 * HOW:  Allocates hash table for key-value pairs.
 *
 * @return Context instance or NULL on error
 */
nimcp_policy_context_t nimcp_policy_context_create(void);

/**
 * Destroy evaluation context
 *
 * WHAT: Destroys an evaluation context.
 * WHY:  Frees memory allocated for context values.
 * HOW:  Iterates and frees all stored values.
 *
 * @param ctx Context instance
 */
void nimcp_policy_context_destroy(nimcp_policy_context_t ctx);

/**
 * Set context string value
 *
 * @param ctx Context instance
 * @param key Variable name
 * @param value String value
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_context_set_string(
    nimcp_policy_context_t ctx,
    const char* key,
    const char* value
);

/**
 * Set context integer value
 *
 * @param ctx Context instance
 * @param key Variable name
 * @param value Integer value
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_context_set_int(
    nimcp_policy_context_t ctx,
    const char* key,
    int64_t value
);

/**
 * Set context float value
 *
 * @param ctx Context instance
 * @param key Variable name
 * @param value Float value
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_context_set_float(
    nimcp_policy_context_t ctx,
    const char* key,
    double value
);

/**
 * Set context boolean value
 *
 * @param ctx Context instance
 * @param key Variable name
 * @param value Boolean value
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_context_set_bool(
    nimcp_policy_context_t ctx,
    const char* key,
    bool value
);

/**
 * Get context value
 *
 * @param ctx Context instance
 * @param key Variable name
 * @param value Output value
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_context_get(
    nimcp_policy_context_t ctx,
    const char* key,
    nimcp_policy_value_t* value
);

/* ========================================================================
 * Policy Evaluation
 * ======================================================================== */

/**
 * Evaluate policies
 *
 * WHAT: Evaluates all loaded policies against a context.
 * WHY:  Determines what action should be taken based on policy rules.
 * HOW:  Executes bytecode interpreter, applies conflict resolution, returns result.
 *
 * @param engine Policy engine instance
 * @param ctx Evaluation context
 * @param result Output evaluation result
 * @return NIMCP_OK on success, error code on failure
 */
nimcp_error_t nimcp_policy_evaluate(
    nimcp_policy_engine_t engine,
    nimcp_policy_context_t ctx,
    nimcp_policy_result_t* result
);

/**
 * Free policy result
 *
 * @param result Result to free
 */
void nimcp_policy_result_free(nimcp_policy_result_t* result);

/* ========================================================================
 * Built-in Functions
 * ======================================================================== */

/**
 * Register custom function
 *
 * WHAT: Registers a custom function callable from policies.
 * WHY:  Allows extending the policy language with custom logic.
 * HOW:  Stores function pointer in engine's function table.
 *
 * @param engine Policy engine instance
 * @param name Function name
 * @param func Function implementation
 * @param user_data User data passed to function
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_register_function(
    nimcp_policy_engine_t engine,
    const char* name,
    nimcp_policy_function_t func,
    void* user_data
);

/* ========================================================================
 * Event Handling
 * ======================================================================== */

/**
 * Register event callback
 *
 * @param engine Policy engine instance
 * @param callback Callback function
 * @param user_data User data passed to callback
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_register_callback(
    nimcp_policy_engine_t engine,
    nimcp_policy_event_callback_t callback,
    void* user_data
);

/* ========================================================================
 * Statistics
 * ======================================================================== */

/**
 * Get engine statistics
 *
 * @param engine Policy engine instance
 * @param stats Output statistics
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_engine_get_stats(
    nimcp_policy_engine_t engine,
    nimcp_policy_stats_t* stats
);

/**
 * Reset statistics
 *
 * @param engine Policy engine instance
 * @return NIMCP_OK on success
 */
nimcp_error_t nimcp_policy_engine_reset_stats(
    nimcp_policy_engine_t engine
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_POLICY_ENGINE_H */
