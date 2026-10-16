/**
 * @file nimcp_training_module.c
 * @brief Training Module Integration Implementation
 *
 * Phase TM-1: Training Module Infrastructure
 *
 * Implements unified infrastructure for all training/plasticity modules
 * with security and memory pool integration.
 *
 * @version 1.0.0
 * @author NIMCP Training Team
 * @date 2025-11-27
 */

#include "middleware/training/nimcp_training_module.h"
#include "constants/nimcp_constants.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

/* Use NOT_IMPLEMENTED since NOT_SUPPORTED doesn't exist */
#ifndef NIMCP_NOT_SUPPORTED
#define NIMCP_NOT_SUPPORTED NIMCP_NOT_IMPLEMENTED
#define LOG_MODULE "training_module"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_module)

#endif

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal training module context
 */
struct nimcp_training_context {
    /* Configuration */
    nimcp_training_module_config_t config;
    nimcp_training_state_t state;
    char name[NIMCP_ID_BUFFER_SIZE];

    /* Security integration */
    nimcp_sec_integration_t* security_ctx;
    uint32_t security_module_id;
    bool owns_security_ctx;

    /* Unified memory integration */
    unified_mem_manager_t mem_manager;
    bool owns_mem_manager;

    /* Statistics */
    nimcp_training_stats_t stats;
    uint64_t init_time;

    /* Weight tracking */
    nimcp_training_weights_t** tracked_weights;
    size_t tracked_weights_count;
    size_t tracked_weights_capacity;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;
};

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static const char* module_type_names[] = {
    "STDP",
    "Dendritic",
    "Predictive",
    "BCM",
    "Homeostatic",
    "Eligibility",
    "BrainLearning",
    "Biological"
};

static const char* phase_names[] = {
    "T1-Homeostatic",
    "T2-Dendritic",
    "T3-Predictive",
    "T4-MetaLearning"
};

static const char* state_names[] = {
    "Uninitialized",
    "Initialized",
    "Active",
    "Paused",
    "Error"
};

//=============================================================================
// Lifecycle Functions
//=============================================================================

nimcp_training_module_config_t nimcp_training_default_config(void)
{
    nimcp_training_module_config_t cfg = {0};
    cfg.type = NIMCP_TRAIN_MOD_BRAIN_LEARNING;
    cfg.name = "training_module";
    cfg.enable_security = true;
    cfg.security_ctx = NULL;
    cfg.enable_unified_memory = true;
    cfg.mem_manager = NULL;
    cfg.weight_pool_size = 0;  /* Use defaults */
    cfg.enable_cow = true;
    cfg.phase = NIMCP_TRAIN_PHASE_T1;
    cfg.learning_rate = NIMCP_LEARNING_RATE_FINE;
    cfg.momentum = NIMCP_MOMENTUM_DEFAULT;
    cfg.weight_decay = NIMCP_WEIGHT_DECAY_DEFAULT;
    cfg.user_data = NULL;
    return cfg;
}

nimcp_training_context_t* nimcp_training_create(
    const nimcp_training_module_config_t* config)
{
    nimcp_training_context_t* ctx = nimcp_calloc(1, sizeof(nimcp_training_context_t));
    NIMCP_API_CHECK_ALLOC_SIZE(ctx, sizeof(nimcp_training_context_t),
        "nimcp_training_create: failed to allocate training context");

    /* Apply configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = nimcp_training_default_config();
    }

    /* Copy name */
    if (ctx->config.name) {
        strncpy(ctx->name, ctx->config.name, sizeof(ctx->name) - 1);
    } else {
        strncpy(ctx->name, "training_module", sizeof(ctx->name) - 1);
    }

    /* Initialize weight tracking */
    ctx->tracked_weights_capacity = 64;
    ctx->tracked_weights = nimcp_calloc(ctx->tracked_weights_capacity,
                                        sizeof(nimcp_training_weights_t*));
    if (!ctx->tracked_weights) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "nimcp_training_create: failed to allocate weight tracking");
        nimcp_free(ctx);
        return NULL;
    }

    ctx->state = NIMCP_TRAIN_STATE_UNINITIALIZED;
    ctx->init_time = get_timestamp_ns();

    return ctx;
}

nimcp_result_t nimcp_training_init(nimcp_training_context_t* ctx)
{
    if (!ctx) return NIMCP_INVALID_PARAM;
    if (ctx->state != NIMCP_TRAIN_STATE_UNINITIALIZED) {
        return NIMCP_ALREADY_EXISTS;
    }

    /* === Setup Security Integration === */
    if (ctx->config.enable_security) {
        if (ctx->config.security_ctx) {
            /* Use provided security context */
            ctx->security_ctx = ctx->config.security_ctx;
            ctx->owns_security_ctx = false;
        } else {
            /* Create own security context */
            ctx->security_ctx = nimcp_sec_integration_create();
            if (!ctx->security_ctx) {
                LOG_WARNING("Failed to create security context for %s", ctx->name);
            } else {
                nimcp_sec_integration_config_t sec_cfg = nimcp_sec_integration_default_config();
                sec_cfg.enable_memory_pools = true;
                sec_cfg.enable_cow = true;
                if (nimcp_sec_integration_init(ctx->security_ctx, &sec_cfg) != NIMCP_SUCCESS) {
                    nimcp_sec_integration_destroy(ctx->security_ctx);
                    ctx->security_ctx = NULL;
                    LOG_WARNING("Failed to init security context for %s", ctx->name);
                } else {
                    ctx->owns_security_ctx = true;
                }
            }
        }

        /* Register module with security */
        if (ctx->security_ctx) {
            nimcp_result_t reg_result = nimcp_sec_register_module(
                ctx->security_ctx,
                ctx->name,
                NIMCP_SEC_CAT_PLASTICITY,
                &ctx->security_module_id
            );
            if (reg_result != NIMCP_SUCCESS) {
                LOG_WARNING("Failed to register %s with security: %d",
                              ctx->name, reg_result);
            } else {
                LOG_INFO("Registered %s with security (module_id=%u)",
                             ctx->name, ctx->security_module_id);
            }
        }
    }

    /* === Setup Unified Memory === */
    if (ctx->config.enable_unified_memory) {
        if (ctx->config.mem_manager) {
            /* Use provided memory manager */
            ctx->mem_manager = ctx->config.mem_manager;
            ctx->owns_mem_manager = false;
        } else {
            /* Create own memory manager */
            unified_mem_config_t mem_cfg = unified_mem_default_config();
            mem_cfg.enable_cow = ctx->config.enable_cow;
            mem_cfg.enable_tracking = true;

            /* Configure pool sizes based on weight_pool_size */
            if (ctx->config.weight_pool_size > 0) {
                /* Calculate appropriate pool sizes */
                mem_cfg.object_pool_num_blocks = ctx->config.weight_pool_size / 4096;
                if (mem_cfg.object_pool_num_blocks < 64) {
                    mem_cfg.object_pool_num_blocks = 64;
                }
            }

            ctx->mem_manager = unified_mem_create(&mem_cfg);
            if (!ctx->mem_manager) {
                LOG_WARNING("Failed to create memory manager for %s", ctx->name);
            } else {
                ctx->owns_mem_manager = true;
                LOG_INFO("Created unified memory manager for %s", ctx->name);
            }
        }
    }

    /* === Setup Bio-async Integration === */
    ctx->bio_ctx = NULL;
    ctx->bio_async_enabled = false;
    if (bio_router_is_initialized()) {
        bio_module_info_t bio_info = {
            .module_id = BIO_MODULE_TRAINING_MODULE,
            .module_name = ctx->name,
            .inbox_capacity = 32,
            .user_data = ctx
        };
        ctx->bio_ctx = bio_router_register_module(&bio_info);
        if (ctx->bio_ctx) {
            ctx->bio_async_enabled = true;
            LOG_INFO("Registered %s with bio-async router", ctx->name);
        }
    }

    ctx->state = NIMCP_TRAIN_STATE_INITIALIZED;
    ctx->stats.security_module_id = ctx->security_module_id;

    LOG_INFO("Training module %s initialized (type=%s, phase=%s)",
                  ctx->name,
                  nimcp_training_type_name(ctx->config.type),
                  nimcp_training_phase_name(ctx->config.phase));

    return NIMCP_SUCCESS;
}

void nimcp_training_destroy(nimcp_training_context_t* ctx)
{
    if (!ctx) return;

    LOG_INFO("Destroying training module %s", ctx->name);

    /* Unregister from bio-async router */
    if (ctx->bio_async_enabled && ctx->bio_ctx) {
        bio_router_unregister_module(ctx->bio_ctx);
        ctx->bio_ctx = NULL;
        ctx->bio_async_enabled = false;
    }

    /* Unregister from security */
    if (ctx->security_ctx && ctx->security_module_id > 0) {
        nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
    }

    /* Free all tracked weights */
    for (size_t i = 0; i < ctx->tracked_weights_count; i++) {
        if (ctx->tracked_weights[i]) {
            nimcp_training_free_weights(ctx, ctx->tracked_weights[i]);
        }
    }
    nimcp_free(ctx->tracked_weights);

    /* Destroy memory manager if we own it */
    if (ctx->mem_manager && ctx->owns_mem_manager) {
        unified_mem_destroy(ctx->mem_manager);
    }

    /* Destroy security context if we own it */
    if (ctx->security_ctx && ctx->owns_security_ctx) {
        nimcp_sec_integration_destroy(ctx->security_ctx);
    }

    nimcp_free(ctx);
}

//=============================================================================
// Weight Management
//=============================================================================

nimcp_result_t nimcp_training_alloc_weights(
    nimcp_training_context_t* ctx,
    size_t num_weights,
    const float* initial_weights,
    nimcp_training_weights_t* weights)
{
    if (!ctx || !weights || num_weights == 0) {
        return NIMCP_INVALID_PARAM;
    }

    if (ctx->state == NIMCP_TRAIN_STATE_UNINITIALIZED) {
        return NIMCP_NOT_INITIALIZED;
    }

    memset(weights, 0, sizeof(nimcp_training_weights_t));
    weights->num_weights = num_weights;
    weights->dimensions[0] = num_weights;
    weights->num_dims = 1;

    size_t size_bytes = num_weights * sizeof(float);
    uint64_t start = get_timestamp_ns();

    if (ctx->mem_manager) {
        /* Use unified memory with CoW */
        unified_mem_request_t req;
        if (ctx->config.enable_cow) {
            req = unified_mem_request_page_cow(size_bytes, initial_weights);
        } else {
            req = unified_mem_request_direct(size_bytes);
        }

        weights->handle = unified_mem_alloc(ctx->mem_manager, &req);
        if (!weights->handle) {
            LOG_ERROR("Failed to allocate %zu weights via unified memory", num_weights);
            return NIMCP_NO_MEMORY;
        }

        /* Copy initial data if provided and using direct mode */
        if (initial_weights && !ctx->config.enable_cow) {
            void* data = unified_mem_write(weights->handle);
            if (data) {
                memcpy(data, initial_weights, size_bytes);
            }
        }
    } else {
        /* Fallback to direct allocation */
        float* data = nimcp_malloc(size_bytes);
        if (!data) {
            LOG_ERROR("Failed to allocate %zu weights", num_weights);
            return NIMCP_NO_MEMORY;
        }
        if (initial_weights) {
            memcpy(data, initial_weights, size_bytes);
        } else {
            memset(data, 0, size_bytes);
        }
        /* Store raw pointer in handle field (cast for storage) */
        weights->handle = (unified_mem_handle_t)data;
    }

    uint64_t elapsed = get_timestamp_ns() - start;
    ctx->stats.alloc_time_ns += elapsed;
    ctx->stats.weights_allocated += size_bytes;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_training_alloc_weights_nd(
    nimcp_training_context_t* ctx,
    const size_t* dimensions,
    uint32_t num_dims,
    const float* initial_weights,
    nimcp_training_weights_t* weights)
{
    if (!ctx || !dimensions || !weights || num_dims == 0 || num_dims > 4) {
        return NIMCP_INVALID_PARAM;
    }

    /* Calculate total number of weights */
    size_t total = 1;
    for (uint32_t i = 0; i < num_dims; i++) {
        if (dimensions[i] == 0) return NIMCP_INVALID_PARAM;
        total *= dimensions[i];
    }

    /* Allocate using 1D function */
    nimcp_result_t result = nimcp_training_alloc_weights(ctx, total, initial_weights, weights);
    if (result != NIMCP_SUCCESS) {
        return result;
    }

    /* Store dimension info */
    weights->num_dims = num_dims;
    for (uint32_t i = 0; i < num_dims; i++) {
        weights->dimensions[i] = dimensions[i];
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_training_clone_weights(
    nimcp_training_context_t* ctx,
    const nimcp_training_weights_t* source,
    nimcp_training_weights_t* dest)
{
    if (!ctx || !source || !dest) {
        return NIMCP_INVALID_PARAM;
    }

    uint64_t start = get_timestamp_ns();

    /* Copy metadata */
    *dest = *source;
    dest->region_id = 0;  /* Clone doesn't inherit security registration */

    if (ctx->mem_manager && source->handle) {
        /* Use CoW clone */
        dest->handle = unified_mem_clone(source->handle);
        if (!dest->handle) {
            return NIMCP_NO_MEMORY;
        }
        ctx->stats.cow_triggers++;
    } else if (source->handle) {
        /* Fallback: deep copy */
        size_t size_bytes = source->num_weights * sizeof(float);
        float* data = nimcp_malloc(size_bytes);
        if (!data) return NIMCP_NO_MEMORY;
        memcpy(data, (void*)source->handle, size_bytes);
        dest->handle = (unified_mem_handle_t)data;
    }

    uint64_t elapsed = get_timestamp_ns() - start;
    ctx->stats.cow_time_ns += elapsed;

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_training_free_weights(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights)
{
    if (!ctx || !weights) {
        return NIMCP_INVALID_PARAM;
    }

    /* Unregister from security if registered */
    if (weights->region_id > 0 && ctx->security_ctx) {
        nimcp_sec_unregister_region(ctx->security_ctx, weights->region_id);
    }

    if (weights->handle) {
        if (ctx->mem_manager) {
            unified_mem_free(weights->handle);
        } else {
            /* Direct allocation fallback */
            nimcp_free((void*)weights->handle);
        }
    }

    size_t size_bytes = weights->num_weights * sizeof(float);
    if (ctx->stats.weights_allocated >= size_bytes) {
        ctx->stats.weights_allocated -= size_bytes;
    }

    memset(weights, 0, sizeof(nimcp_training_weights_t));
    return NIMCP_SUCCESS;
}

const float* nimcp_training_read_weights(
    nimcp_training_context_t* ctx,
    const nimcp_training_weights_t* weights)
{
    if (!ctx || !weights || !weights->handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_training_read_weights: required parameter is NULL (ctx, weights, weights->handle)");
        return NULL;
    }

    if (ctx->mem_manager) {
        return (const float*)unified_mem_read(weights->handle);
    } else {
        /* Direct allocation fallback */
        return (const float*)weights->handle;
    }
}

float* nimcp_training_write_weights(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights)
{
    if (!ctx || !weights || !weights->handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_training_write_weights: required parameter is NULL (ctx, weights, weights->handle)");
        return NULL;
    }

    if (weights->is_frozen) {
        LOG_WARNING("Attempted to write to frozen weights");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "nimcp_training_write_weights: weights are frozen");
        return NULL;
    }

    uint64_t start = get_timestamp_ns();
    float* data;

    if (ctx->mem_manager) {
        data = (float*)unified_mem_write(weights->handle);
        if (unified_mem_is_shared(weights->handle) == false) {
            /* CoW was triggered */
            ctx->stats.cow_triggers++;
        }
    } else {
        /* Direct allocation fallback */
        data = (float*)weights->handle;
    }

    uint64_t elapsed = get_timestamp_ns() - start;
    ctx->stats.cow_time_ns += elapsed;

    return data;
}

bool nimcp_training_weights_are_shared(const nimcp_training_weights_t* weights)
{
    if (!weights || !weights->handle) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_training_weights_are_shared: required parameter is NULL (weights, weights->handle)");
        return false;
    }
    return unified_mem_is_shared(weights->handle);
}

nimcp_result_t nimcp_training_register_weights_security(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    const char* name)
{
    if (!ctx || !weights || !name) {
        return NIMCP_INVALID_PARAM;
    }

    if (!ctx->security_ctx || ctx->security_module_id == 0) {
        return NIMCP_NOT_INITIALIZED;
    }

    const float* data = nimcp_training_read_weights(ctx, weights);
    if (!data) {
        return NIMCP_INVALID_STATE;
    }

    size_t size_bytes = weights->num_weights * sizeof(float);

    nimcp_result_t result = nimcp_sec_register_region(
        ctx->security_ctx,
        ctx->security_module_id,
        name,
        data,
        size_bytes,
        &weights->region_id
    );

    if (result == NIMCP_SUCCESS) {
        LOG_DEBUG("Registered weights '%s' with security (region_id=%u)",
                       name, weights->region_id);
    }

    return result;
}

nimcp_result_t nimcp_training_update_weights_baseline(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights)
{
    if (!ctx || !weights) {
        return NIMCP_INVALID_PARAM;
    }

    if (!ctx->security_ctx || weights->region_id == 0) {
        return NIMCP_NOT_INITIALIZED;
    }

    const float* data = nimcp_training_read_weights(ctx, weights);
    if (!data) {
        return NIMCP_INVALID_STATE;
    }

    size_t size_bytes = weights->num_weights * sizeof(float);

    return nimcp_sec_update_region_baseline(
        ctx->security_ctx,
        weights->region_id,
        data,
        size_bytes
    );
}

//=============================================================================
// Checkpoint API
//=============================================================================

nimcp_result_t nimcp_training_checkpoint_create(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    size_t num_weights,
    nimcp_training_checkpoint_t* checkpoint)
{
    if (!ctx || !weights || !checkpoint || num_weights == 0) {
        return NIMCP_INVALID_PARAM;
    }

    if (!ctx->mem_manager) {
        /* Checkpointing requires unified memory */
        return NIMCP_NOT_SUPPORTED;
    }

    memset(checkpoint, 0, sizeof(nimcp_training_checkpoint_t));

    checkpoint->snapshots = nimcp_calloc(num_weights, sizeof(unified_mem_snapshot_t));
    if (!checkpoint->snapshots) {
        return NIMCP_NO_MEMORY;
    }

    for (size_t i = 0; i < num_weights; i++) {
        if (weights[i].handle) {
            checkpoint->snapshots[i] = unified_mem_snapshot_create(weights[i].handle);
            if (!checkpoint->snapshots[i]) {
                /* Cleanup on failure */
                for (size_t j = 0; j < i; j++) {
                    if (checkpoint->snapshots[j]) {
                        unified_mem_snapshot_destroy(checkpoint->snapshots[j]);
                    }
                }
                nimcp_free(checkpoint->snapshots);
                memset(checkpoint, 0, sizeof(nimcp_training_checkpoint_t));
                return NIMCP_NO_MEMORY;
            }
        }
    }

    checkpoint->num_snapshots = num_weights;
    checkpoint->step_number = ctx->stats.training_steps;
    checkpoint->timestamp = get_timestamp_ns();

    return NIMCP_SUCCESS;
}

nimcp_result_t nimcp_training_checkpoint_restore(
    nimcp_training_context_t* ctx,
    nimcp_training_weights_t* weights,
    size_t num_weights,
    const nimcp_training_checkpoint_t* checkpoint)
{
    if (!ctx || !weights || !checkpoint || num_weights == 0) {
        return NIMCP_INVALID_PARAM;
    }

    if (num_weights != checkpoint->num_snapshots) {
        return NIMCP_INVALID_PARAM;
    }

    for (size_t i = 0; i < num_weights; i++) {
        if (weights[i].handle && checkpoint->snapshots[i]) {
            if (!unified_mem_snapshot_restore(weights[i].handle, checkpoint->snapshots[i])) {
                return NIMCP_ERROR;
            }
        }
    }

    return NIMCP_SUCCESS;
}

void nimcp_training_checkpoint_destroy(
    nimcp_training_context_t* ctx,
    nimcp_training_checkpoint_t* checkpoint)
{
    if (!checkpoint) return;
    (void)ctx;  /* ctx might be needed for future cleanup */

    if (checkpoint->snapshots) {
        for (size_t i = 0; i < checkpoint->num_snapshots; i++) {
            if (checkpoint->snapshots[i]) {
                unified_mem_snapshot_destroy(checkpoint->snapshots[i]);
            }
        }
        nimcp_free(checkpoint->snapshots);
    }

    memset(checkpoint, 0, sizeof(nimcp_training_checkpoint_t));
}

//=============================================================================
// Security Integration
//=============================================================================

nimcp_result_t nimcp_training_record_success(nimcp_training_context_t* ctx)
{
    if (!ctx || !ctx->security_ctx || ctx->security_module_id == 0) {
        return NIMCP_NOT_INITIALIZED;
    }
    return nimcp_sec_record_interaction(ctx->security_ctx, ctx->security_module_id, true, 1.0);
}

nimcp_result_t nimcp_training_record_failure(nimcp_training_context_t* ctx)
{
    if (!ctx || !ctx->security_ctx || ctx->security_module_id == 0) {
        return NIMCP_NOT_INITIALIZED;
    }
    return nimcp_sec_record_interaction(ctx->security_ctx, ctx->security_module_id, false, 1.0);
}

bool nimcp_training_is_trusted(nimcp_training_context_t* ctx)
{
    if (!ctx || !ctx->security_ctx || ctx->security_module_id == 0) {
        return true;  /* Default to trusted if security not enabled */
    }
    return nimcp_sec_is_module_trusted(ctx->security_ctx, ctx->security_module_id);
}

uint32_t nimcp_training_get_security_id(nimcp_training_context_t* ctx)
{
    if (!ctx) return 0;
    return ctx->security_module_id;
}

nimcp_sec_integration_t* nimcp_training_get_security_ctx(nimcp_training_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_training_get_security_ctx: ctx is NULL");
        return NULL;
    }
    return ctx->security_ctx;
}

//=============================================================================
// Statistics and State
//=============================================================================

nimcp_result_t nimcp_training_get_stats(
    nimcp_training_context_t* ctx,
    nimcp_training_stats_t* stats)
{
    if (!ctx || !stats) {
        return NIMCP_INVALID_PARAM;
    }

    *stats = ctx->stats;

    /* Update trust score if security is enabled */
    if (ctx->security_ctx && ctx->security_module_id > 0) {
        nimcp_trust_score_t score;
        if (nimcp_sec_get_trust_score(ctx->security_ctx, ctx->security_module_id, &score) == NIMCP_SUCCESS) {
            stats->trust_score = score.expected_trust;
        }
    }

    /* Update memory stats if unified memory is enabled */
    if (ctx->mem_manager) {
        unified_mem_stats_t mem_stats;
        if (unified_mem_get_stats(ctx->mem_manager, &mem_stats)) {
            stats->weights_shared = mem_stats.shared_memory_bytes;
            stats->memory_saved = mem_stats.memory_saved_bytes;
        }
    }

    return NIMCP_SUCCESS;
}

void nimcp_training_reset_stats(nimcp_training_context_t* ctx)
{
    if (!ctx) return;

    uint32_t module_id = ctx->stats.security_module_id;
    memset(&ctx->stats, 0, sizeof(nimcp_training_stats_t));
    ctx->stats.security_module_id = module_id;
}

nimcp_training_state_t nimcp_training_get_state(nimcp_training_context_t* ctx)
{
    if (!ctx) return NIMCP_TRAIN_STATE_UNINITIALIZED;
    return ctx->state;
}

nimcp_training_module_type_t nimcp_training_get_type(nimcp_training_context_t* ctx)
{
    if (!ctx) return NIMCP_TRAIN_MOD_BRAIN_LEARNING;
    return ctx->config.type;
}

unified_mem_manager_t nimcp_training_get_mem_manager(nimcp_training_context_t* ctx)
{
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "nimcp_training_get_mem_manager: ctx is NULL");
        return NULL;
    }
    return ctx->mem_manager;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* nimcp_training_type_name(nimcp_training_module_type_t type)
{
    if (type < NIMCP_TRAIN_MOD_COUNT) {
        return module_type_names[type];
    }
    return "Unknown";
}

const char* nimcp_training_phase_name(nimcp_training_phase_t phase)
{
    if (phase < NIMCP_TRAIN_PHASE_COUNT) {
        return phase_names[phase];
    }
    return "Unknown";
}

const char* nimcp_training_state_name(nimcp_training_state_t state)
{
    if (state <= NIMCP_TRAIN_STATE_ERROR) {
        return state_names[state];
    }
    return "Unknown";
}
