/**
 * @file nimcp_policy_engine.c
 * @brief NIMCP Policy Engine Implementation
 *
 * WHAT: Main policy engine coordinating parsing, compilation, and evaluation.
 * WHY:  Provides unified interface for security policy management.
 * HOW:  Integrates parser, compiler, evaluator with bio-async and logging.
 */

#include "security/nimcp_policy_engine.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_wiring_helpers.h"

#include "security/nimcp_policy_parser.h"
#include "utils/validation/nimcp_common.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>  /* for NULL */
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(policy_engine, MESH_ADAPTER_CATEGORY_SECURITY)


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
#ifndef NIMCP_ERROR_IO
#define NIMCP_ERROR_IO (-121)
#endif
#ifndef NIMCP_ERROR_PARTIAL
#define NIMCP_ERROR_PARTIAL (-122)
#endif

/* Forward declarations from other modules */
typedef struct bytecode bytecode_t;
typedef struct function_registry function_registry_t;
struct nimcp_ast_node;
typedef struct nimcp_ast_node nimcp_ast_node_t;

/* AST destruction - forward declare from nimcp_policy_ast.h */
extern void nimcp_ast_destroy(nimcp_ast_node_t* node);

/* AST destruction wrapper for void* pointers */
static inline void ast_destroy(void* ast) {
    nimcp_ast_destroy((nimcp_ast_node_t*)ast);
}

extern bytecode_t* nimcp_policy_compile(const void* ast);
extern void nimcp_policy_bytecode_destroy(bytecode_t* bc);
extern nimcp_error_t nimcp_policy_evaluate_bytecode(
    bytecode_t* bc,
    nimcp_policy_context_t ctx,
    function_registry_t* registry,
    nimcp_policy_result_t* result
);
extern function_registry_t* registry_create(void);
extern void registry_destroy(function_registry_t* registry);
extern nimcp_error_t registry_add(
    function_registry_t* registry,
    const char* name,
    nimcp_policy_function_t func,
    void* user_data
);

/* ========================================================================
 * Internal Structures
 * ======================================================================== */

typedef struct nimcp_policy {
    char* name;
    char* source_file;
    void* ast;  // nimcp_ast_node_t*
    bytecode_t* bytecode;
    uint64_t version;
    struct nimcp_policy* next;
} policy_internal_t;

typedef struct {
    nimcp_policy_event_callback_t callback;
    void* user_data;
} event_callback_entry_t;

struct nimcp_policy_engine {
    uint32_t magic;
    nimcp_policy_engine_config_t config;
    policy_internal_t* policies;
    size_t num_policies;
    function_registry_t* function_registry;
    event_callback_entry_t* callbacks;
    size_t num_callbacks;
    nimcp_policy_stats_t stats;
    nimcp_mutex_t lock;
    bool bio_async_registered;
    bio_module_context_t bio_ctx;
};

#define ENGINE_MAGIC 0x504F4C45  /* 'POLE' */

/* ========================================================================
 * Bio-Async Integration
 * ======================================================================== */

/**
 * @brief Wiring callback for KG-driven handler registration
 *
 * Called by the orchestrator with discovered message types from the knowledge graph.
 * Registers handlers based on message types discovered at runtime.
 *
 * @param ctx Bio-async module context
 * @param message_types Array of discovered message types
 * @param message_count Number of message types
 * @param user_data User-provided context
 * @return 0 on success, -1 on error
 */
static int policy_engine_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
);

/**
 * Message handler for bio-async messages
 */
static nimcp_error_t policy_engine_message_handler(
    const void* msg,
    size_t msg_size,
    nimcp_bio_promise_t response_promise,
    void* user_data)
{
    (void)response_promise;
    nimcp_policy_engine_t engine = (nimcp_policy_engine_t)user_data;

    if (!engine || engine->magic != ENGINE_MAGIC) {
        LOG_ERROR("Invalid policy engine in message handler");
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!msg || msg_size == 0) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_DEBUG("Policy engine received message size %zu", msg_size);

    /* Process message based on header type */
    const bio_message_header_t* header = (const bio_message_header_t*)msg;

    if (header->type == BIO_MSG_SECURITY_POLICY_UPDATE) {
        LOG_INFO("Received policy update request");
        nimcp_policy_engine_reload(engine);
    }

    return NIMCP_SUCCESS;
}

/**
 * @brief Wiring callback implementation for KG-driven handler registration
 */
static int policy_engine_wiring_handler_callback(
    bio_module_context_t ctx,
    const bio_message_type_t* message_types,
    uint32_t message_count,
    void* user_data
) {
    (void)user_data;

    int registered = 0;
    for (uint32_t i = 0; i < message_count; i++) {
        switch (message_types[i]) {
            case BIO_MSG_SECURITY_POLICY_UPDATE:
                bio_router_register_handler(ctx, message_types[i], policy_engine_message_handler);
                registered++;
                break;
            default:
                LOG_DEBUG("Unknown message type %d in wiring callback", message_types[i]);
                break;
        }
    }

    LOG_INFO("KG-driven wiring callback registered %d handlers", registered);
    return (registered > 0) ? 0 : -1;
}

/* ========================================================================
 * Engine Lifecycle
 * ======================================================================== */

nimcp_policy_engine_t nimcp_policy_engine_create(
    const nimcp_policy_engine_config_t* config)
{
    LOG_INFO("Creating policy engine");

    if (!config) {
        LOG_ERROR("NULL configuration provided");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_policy_engine_create: config is NULL");
        return NULL;
    }

    struct nimcp_policy_engine* engine = nimcp_calloc(1, sizeof(struct nimcp_policy_engine));
    NIMCP_API_CHECK_ALLOC(engine, "Failed to allocate policy engine");

    engine->magic = ENGINE_MAGIC;
    engine->config = *config;
    engine->function_registry = registry_create();
    nimcp_mutex_init(&engine->lock, NULL);

    if (!engine->function_registry) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to create function registry for policy engine");
        nimcp_free(engine);
        return NULL;
    }

    /* Register with bio-async if router provided */
    if (config->bio_router) {
        bio_module_info_t info = {
            .module_id = BIO_MODULE_SECURITY,
            .module_name = "policy_engine",
            .inbox_capacity = 32,
            .user_data = engine
        };
        engine->bio_ctx = bio_router_register_module(&info);
        if (engine->bio_ctx) {
            // Try KG-driven wiring callback registration first
            nimcp_error_t result = bio_router_register_wiring_callback(
                BIO_MODULE_SECURITY,
                (void*)policy_engine_wiring_handler_callback,
                engine
            );

            if (result == NIMCP_SUCCESS) {
                LOG_INFO("KG-driven wiring callback registered successfully");
            } else {
                // Fallback to legacy handler registration
                LOG_INFO("Falling back to legacy handler registration");

                LEGACY_HANDLER_REGISTRATION(
                    bio_router_register_handler(engine->bio_ctx,
                                                BIO_MSG_SECURITY_POLICY_UPDATE,
                                                policy_engine_message_handler)
                );
            }

            engine->bio_async_registered = true;
            LOG_INFO("Policy engine registered with bio-async");
        } else {
            LOG_WARN("Failed to register policy engine with bio-async");
        }
    }

    LOG_INFO("Policy engine created successfully");
    return engine;
}

void nimcp_policy_engine_destroy(nimcp_policy_engine_t engine) {
    if (!engine || engine->magic != ENGINE_MAGIC) {
        LOG_ERROR("Invalid policy engine");
        return;
    }

    LOG_INFO("Destroying policy engine");

    nimcp_mutex_lock(&engine->lock);

    // Unload all policies
    policy_internal_t* policy = engine->policies;
    while (policy) {
        policy_internal_t* next = policy->next;
        nimcp_free(policy->name);
        nimcp_free(policy->source_file);
        if (policy->ast) {
            
            ast_destroy(policy->ast);
        }
        if (policy->bytecode) {
            nimcp_policy_bytecode_destroy(policy->bytecode);
        }
        nimcp_free(policy);
        policy = next;
    }

    // Destroy function registry
    if (engine->function_registry) {
        registry_destroy(engine->function_registry);
    }

    // Free callbacks
    nimcp_free(engine->callbacks);

    // Unregister from bio-async
    if (engine->bio_async_registered && engine->bio_ctx) {
        bio_router_unregister_module(engine->bio_ctx);
        engine->bio_ctx = NULL;
        engine->bio_async_registered = false;
    }

    nimcp_mutex_unlock(&engine->lock);
    nimcp_mutex_destroy(&engine->lock);

    engine->magic = 0;
    nimcp_free(engine);

    LOG_INFO("Policy engine destroyed");
}

/* ========================================================================
 * Policy Management
 * ======================================================================== */

nimcp_error_t nimcp_policy_engine_load(
    nimcp_policy_engine_t engine,
    const char* policy_text,
    nimcp_policy_t* policy)
{
    if (!engine || engine->magic != ENGINE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!policy_text || !policy) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Loading policy from text");

    nimcp_mutex_lock(&engine->lock);

    // Parse
    char* error_msg = NULL;
    void* ast = nimcp_policy_parse(policy_text, "(string)", &error_msg);
    if (!ast) {
        LOG_ERROR("Failed to parse policy: %s", error_msg ? error_msg : "unknown error");
        nimcp_free(error_msg);
        engine->stats.parse_errors++;
        nimcp_mutex_unlock(&engine->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Compile
    bytecode_t* bytecode = nimcp_policy_compile(ast);
    if (!bytecode) {
        LOG_ERROR("Failed to compile policy");
        
        ast_destroy(ast);
        nimcp_mutex_unlock(&engine->lock);
        return NIMCP_ERROR_INVALID_PARAM;
    }

    // Create policy object
    policy_internal_t* new_policy = nimcp_calloc(1, sizeof(policy_internal_t));
    if (!new_policy) {
        LOG_ERROR("Failed to allocate policy");
        
        ast_destroy(ast);
        nimcp_policy_bytecode_destroy(bytecode);
        nimcp_mutex_unlock(&engine->lock);
        return NIMCP_ERROR_NO_MEMORY;
    }

    new_policy->name = strdup("policy");
    new_policy->ast = ast;
    new_policy->bytecode = bytecode;
    new_policy->version = 1;

    // Add to list
    new_policy->next = engine->policies;
    engine->policies = new_policy;
    engine->num_policies++;
    engine->stats.num_policies++;

    *policy = (nimcp_policy_t)new_policy;

    LOG_INFO("Successfully loaded policy");

    // Send bio-async event if enabled
    if (engine->bio_async_registered && engine->bio_ctx) {
        // For now, skip sending bio-async messages as proper message structure needs definition
        LOG_DEBUG("Policy loaded (bio-async notification skipped)");
    }

    nimcp_mutex_unlock(&engine->lock);
    return NIMCP_OK;
}

nimcp_error_t nimcp_policy_engine_load_file(
    nimcp_policy_engine_t engine,
    const char* filepath,
    nimcp_policy_t* policy)
{
    if (!engine || engine->magic != ENGINE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (!filepath || !policy) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Loading policy from file: %s", filepath);

    // Read file
    FILE* file = fopen(filepath, "r");
    if (!file) {
        LOG_ERROR("Failed to open policy file: %s", filepath);
        return NIMCP_ERROR_IO;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* content = nimcp_malloc(size + 1);
    if (!content) {
        fclose(file);
        return NIMCP_ERROR_NO_MEMORY;
    }

    size_t read = fread(content, 1, size, file);
    fclose(file);
    if (read != size) {
        nimcp_free(content);
        return NIMCP_ERROR_IO;
    }
    content[read] = '\0';

    // Load policy
    nimcp_error_t result = nimcp_policy_engine_load(engine, content, policy);
    if (result == NIMCP_OK && *policy) {
        policy_internal_t* p = (policy_internal_t*)*policy;
        p->source_file = strdup(filepath);
    }

    nimcp_free(content);
    return result;
}

nimcp_error_t nimcp_policy_engine_unload(
    nimcp_policy_engine_t engine,
    nimcp_policy_t policy)
{
    if (!engine || engine->magic != ENGINE_MAGIC || !policy) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&engine->lock);

    policy_internal_t* target = (policy_internal_t*)policy;
    policy_internal_t** current = &engine->policies;

    while (*current) {
        if (*current == target) {
            *current = target->next;

            nimcp_free(target->name);
            nimcp_free(target->source_file);
            if (target->ast) {
                
                ast_destroy(target->ast);
            }
            if (target->bytecode) {
                nimcp_policy_bytecode_destroy(target->bytecode);
            }
            nimcp_free(target);

            engine->num_policies--;
            LOG_INFO("Policy unloaded");

            nimcp_mutex_unlock(&engine->lock);
            return NIMCP_OK;
        }
        current = &(*current)->next;
    }

    nimcp_mutex_unlock(&engine->lock);
    return NIMCP_ERROR_NOT_FOUND;
}

nimcp_error_t nimcp_policy_engine_reload(nimcp_policy_engine_t engine) {
    if (!engine || engine->magic != ENGINE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Reloading all policies");

    nimcp_mutex_lock(&engine->lock);

    policy_internal_t* policy = engine->policies;
    size_t reloaded = 0;
    size_t errors = 0;

    while (policy) {
        if (policy->source_file) {
            // Read file
            FILE* file = fopen(policy->source_file, "r");
            if (!file) {
                LOG_ERROR("Failed to reload policy from: %s", policy->source_file);
                errors++;
                policy = policy->next;
                continue;
            }

            fseek(file, 0, SEEK_END);
            long size = ftell(file);
            fseek(file, 0, SEEK_SET);

            char* content = nimcp_malloc(size + 1);
            if (!content) {
                fclose(file);
                errors++;
                policy = policy->next;
                continue;
            }

            size_t read = fread(content, 1, size, file);
            fclose(file);
            if (read != size) {
                nimcp_free(content);
                errors++;
                policy = policy->next;
                continue;
            }
            content[read] = '\0';

            // Parse and compile
            char* error_msg = NULL;
            void* ast = nimcp_policy_parse(content, policy->source_file, &error_msg);
            nimcp_free(content);

            if (!ast) {
                LOG_ERROR("Failed to parse policy during reload: %s",
                              error_msg ? error_msg : "unknown");
                nimcp_free(error_msg);
                errors++;
                policy = policy->next;
                continue;
            }

            bytecode_t* bytecode = nimcp_policy_compile(ast);
            if (!bytecode) {
                LOG_ERROR("Failed to compile policy during reload");
                
                ast_destroy(ast);
                errors++;
                policy = policy->next;
                continue;
            }

            // Replace old AST and bytecode
            if (policy->ast) {
                
                ast_destroy(policy->ast);
            }
            if (policy->bytecode) {
                nimcp_policy_bytecode_destroy(policy->bytecode);
            }

            policy->ast = ast;
            policy->bytecode = bytecode;
            policy->version++;

            reloaded++;
            LOG_INFO("Reloaded policy: %s (version %lu)",
                          policy->name, policy->version);
        }

        policy = policy->next;
    }

    LOG_INFO("Reload complete: %zu policies reloaded, %zu errors", reloaded, errors);

    nimcp_mutex_unlock(&engine->lock);
    return errors == 0 ? NIMCP_OK : NIMCP_ERROR_PARTIAL;
}

/* ========================================================================
 * Policy Evaluation
 * ======================================================================== */

nimcp_error_t nimcp_policy_evaluate(
    nimcp_policy_engine_t engine,
    nimcp_policy_context_t ctx,
    nimcp_policy_result_t* result)
{
    if (!engine || engine->magic != ENGINE_MAGIC || !ctx || !result) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_DEBUG("Evaluating policies");

    nimcp_mutex_lock(&engine->lock);

    memset(result, 0, sizeof(nimcp_policy_result_t));
    result->action = NIMCP_POLICY_ACTION_DENY;  // Default deny

    nimcp_error_t error = NIMCP_OK;
    policy_internal_t* policy = engine->policies;

    while (policy) {
        if (policy->bytecode) {
            nimcp_policy_result_t policy_result = {0};

            error = nimcp_policy_evaluate_bytecode(
                policy->bytecode,
                ctx,
                engine->function_registry,
                &policy_result
            );

            if (error == NIMCP_OK) {
                // First match wins
                *result = policy_result;
                engine->stats.total_evaluations++;

                LOG_DEBUG("Policy matched: action=%d", result->action);
                break;
            } else {
                engine->stats.eval_errors++;
            }
        }

        policy = policy->next;
    }

    // Update statistics
    if (result->eval_time_ns > engine->stats.max_eval_time_ns) {
        engine->stats.max_eval_time_ns = result->eval_time_ns;
    }
    engine->stats.avg_eval_time_ns =
        (engine->stats.avg_eval_time_ns * (engine->stats.total_evaluations - 1) +
         result->eval_time_ns) / engine->stats.total_evaluations;

    // Notify callbacks
    for (size_t i = 0; i < engine->num_callbacks; i++) {
        engine->callbacks[i].callback(
            "policy.evaluated",
            result,
            engine->callbacks[i].user_data
        );
    }

    nimcp_mutex_unlock(&engine->lock);
    return error;
}

/* ========================================================================
 * Built-in Functions
 * ======================================================================== */

nimcp_error_t nimcp_policy_register_function(
    nimcp_policy_engine_t engine,
    const char* name,
    nimcp_policy_function_t func,
    void* user_data)
{
    if (!engine || engine->magic != ENGINE_MAGIC || !name || !func) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    LOG_INFO("Registering policy function: %s", name);

    nimcp_mutex_lock(&engine->lock);
    nimcp_error_t result = registry_add(engine->function_registry, name, func, user_data);
    nimcp_mutex_unlock(&engine->lock);

    return result;
}

/* ========================================================================
 * Event Handling
 * ======================================================================== */

nimcp_error_t nimcp_policy_register_callback(
    nimcp_policy_engine_t engine,
    nimcp_policy_event_callback_t callback,
    void* user_data)
{
    if (!engine || engine->magic != ENGINE_MAGIC || !callback) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&engine->lock);

    engine->callbacks = nimcp_realloc(
        engine->callbacks,
        (engine->num_callbacks + 1) * sizeof(event_callback_entry_t)
    );

    if (!engine->callbacks) {
        nimcp_mutex_unlock(&engine->lock);
        return NIMCP_ERROR_NO_MEMORY;
    }

    engine->callbacks[engine->num_callbacks].callback = callback;
    engine->callbacks[engine->num_callbacks].user_data = user_data;
    engine->num_callbacks++;

    nimcp_mutex_unlock(&engine->lock);
    return NIMCP_OK;
}

/* ========================================================================
 * Statistics
 * ======================================================================== */

nimcp_error_t nimcp_policy_engine_get_stats(
    nimcp_policy_engine_t engine,
    nimcp_policy_stats_t* stats)
{
    if (!engine || engine->magic != ENGINE_MAGIC || !stats) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&engine->lock);
    *stats = engine->stats;
    stats->num_policies = engine->num_policies;
    nimcp_mutex_unlock(&engine->lock);

    return NIMCP_OK;
}

nimcp_error_t nimcp_policy_engine_reset_stats(
    nimcp_policy_engine_t engine)
{
    if (!engine || engine->magic != ENGINE_MAGIC) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    nimcp_mutex_lock(&engine->lock);
    memset(&engine->stats, 0, sizeof(nimcp_policy_stats_t));
    nimcp_mutex_unlock(&engine->lock);

    LOG_INFO("Policy engine statistics reset");
    return NIMCP_OK;
}

/* ========================================================================
 * Registry Stub Implementation (for linking)
 * ======================================================================== */

typedef struct {
    char* name;
    nimcp_policy_function_t func;
    void* user_data;
} function_entry_t;

struct function_registry {
    function_entry_t* functions;
    size_t count;
    size_t capacity;
};

function_registry_t* registry_create(void);  // Implemented in evaluator
void registry_destroy(function_registry_t* registry);  // Implemented in evaluator

nimcp_error_t registry_add(
    function_registry_t* registry,
    const char* name,
    nimcp_policy_function_t func,
    void* user_data)
{
    if (!registry || !name || !func) {
        return NIMCP_ERROR_INVALID_PARAM;
    }

    if (registry->count >= registry->capacity) {
        registry->capacity = registry->capacity == 0 ? 16 : registry->capacity * 2;
        registry->functions = nimcp_realloc(
            registry->functions,
            registry->capacity * sizeof(function_entry_t)
        );
    }

    registry->functions[registry->count].name = strdup(name);
    registry->functions[registry->count].func = func;
    registry->functions[registry->count].user_data = user_data;
    registry->count++;

    return NIMCP_OK;
}
