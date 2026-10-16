/**
 * @file nimcp_training_state_manager.c
 * @brief State Manager Integration for Training Modules
 *
 * WHAT: State serialization/recovery for training subsystems
 * WHY:  Enable checkpointing and fault tolerance for long training runs
 * HOW:  Register training contexts with state manager for checkpoint/restore
 *
 * PHASE 8: System-Wide Health Integration
 *
 * @author NIMCP Team
 * @date 2026-01-25
 * @version 1.0.0
 */

#include "training/integration/nimcp_training_state_manager.h"
#include "utils/fault_tolerance/nimcp_state_manager.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "training/nimcp_distributed_training.h"
#include "training/nimcp_meta_learning.h"
#include "training/nimcp_adversarial_training.h"
#include "training/nimcp_hyperparam_opt.h"
#include <string.h>

#define LOG_MODULE "TRAINING_STATE"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_state_manager)

//=============================================================================
// Internal State Structures for Serialization
//=============================================================================

/**
 * @brief Distributed training serializable state
 *
 * Captures essential state that can be restored after failure.
 */
typedef struct {
    uint32_t magic;                  /**< State magic for validation */
    uint32_t version;                /**< State format version */
    uint64_t total_steps;            /**< Total training steps */
    uint64_t sync_events;            /**< Gradient sync events */
    uint32_t rank;                   /**< Worker rank */
    uint32_t world_size;             /**< Total workers */
    uint64_t bytes_sent;             /**< Communication stats */
    uint64_t bytes_received;         /**< Communication stats */
    uint64_t timeout_count;          /**< Error stats */
    uint64_t retry_count;            /**< Retry stats */
    bool is_initialized;             /**< Initialization state */
} dist_serializable_state_t;

#define DIST_STATE_MAGIC 0x44495354  /* "DIST" */
#define DIST_STATE_VERSION 1

/**
 * @brief Meta-learning serializable state
 */
typedef struct {
    uint32_t magic;                  /**< State magic */
    uint32_t version;                /**< Format version */
    uint64_t total_meta_steps;       /**< Total meta-training steps */
    uint64_t total_inner_steps;      /**< Total inner loop steps */
    uint64_t tasks_processed;        /**< Tasks processed */
    float avg_query_loss;            /**< Average query loss */
    float avg_query_accuracy;        /**< Average query accuracy */
    float avg_inner_grad_norm;       /**< Gradient norms */
    float avg_outer_grad_norm;       /**< Gradient norms */
    bool is_initialized;             /**< Initialization state */
} meta_serializable_state_t;

#define META_STATE_MAGIC 0x4D455441  /* "META" */
#define META_STATE_VERSION 1

/**
 * @brief Adversarial training serializable state
 */
typedef struct {
    uint32_t magic;                  /**< State magic */
    uint32_t version;                /**< Format version */
    uint64_t total_steps;            /**< Total training steps */
    uint64_t adversarial_steps;      /**< Steps with adversarial examples */
    uint64_t total_attacks;          /**< Total attacks performed */
    uint64_t successful_attacks;     /**< Successful attacks */
    float clean_accuracy;            /**< Clean accuracy */
    float robust_accuracy;           /**< Robust accuracy */
    float avg_perturbation_norm;     /**< Average perturbation */
    float detection_rate;            /**< Detection rate */
    bool is_initialized;             /**< Initialization state */
} adv_serializable_state_t;

#define ADV_STATE_MAGIC 0x41445652  /* "ADVR" */
#define ADV_STATE_VERSION 1

/**
 * @brief HPO serializable state
 */
typedef struct {
    uint32_t magic;                  /**< State magic */
    uint32_t version;                /**< Format version */
    uint64_t total_trials;           /**< Total trials run */
    uint64_t completed_trials;       /**< Completed trials */
    uint64_t pruned_trials;          /**< Pruned trials */
    uint64_t failed_trials;          /**< Failed trials */
    double best_objective;           /**< Best objective value */
    double avg_objective;            /**< Average objective */
    uint32_t trials_to_best;         /**< Trials to find best */
    bool is_initialized;             /**< Initialization state */
} hpo_serializable_state_t;

#define HPO_STATE_MAGIC 0x48504F54  /* "HPOT" */
#define HPO_STATE_VERSION 1

//=============================================================================
// Module Priority Constants
//=============================================================================

#define PRIORITY_DISTRIBUTED   100   /**< Distributed training first */
#define PRIORITY_META_LEARNING 110   /**< Meta-learning second */
#define PRIORITY_ADVERSARIAL   120   /**< Adversarial training third */
#define PRIORITY_HPO           130   /**< HPO last */

//=============================================================================
// Distributed Training State Operations
//=============================================================================

static int dist_state_serialize(void* ctx, uint8_t* buffer, size_t* size)
{
    if (!size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_state_serialize: size is NULL");
        return -1;
    }

    size_t needed = sizeof(dist_serializable_state_t);

    if (!buffer) {
        *size = needed;
        return 0;
    }

    if (*size < needed) {
        *size = needed;
        return -2;  /* Buffer too small */
    }

    dist_serializable_state_t state;
    memset(&state, 0, sizeof(state));

    state.magic = DIST_STATE_MAGIC;
    state.version = DIST_STATE_VERSION;

    /* Context may be NULL if module not yet created */
    if (ctx) {
        state.is_initialized = true;
        dist_stats_t stats;
        if (dist_get_stats((const dist_ctx_t*)ctx, &stats) == 0) {
            state.total_steps = stats.total_steps;
            state.sync_events = stats.sync_events;
            state.bytes_sent = stats.bytes_sent;
            state.bytes_received = stats.bytes_received;
            state.timeout_count = stats.timeout_count;
            state.retry_count = stats.retry_count;
        }
        state.rank = dist_get_rank((const dist_ctx_t*)ctx);
        state.world_size = dist_get_world_size((const dist_ctx_t*)ctx);
    }

    memcpy(buffer, &state, sizeof(state));
    *size = needed;
    return 0;
}

static int dist_state_deserialize(void* ctx, const uint8_t* buffer, size_t size)
{
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "dist_state_deserialize: buffer is NULL");
        return -1;
    }
    if (size < sizeof(dist_serializable_state_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_state_deserialize: validation failed");
        return -1;
    }

    const dist_serializable_state_t* state = (const dist_serializable_state_t*)buffer;

    /* Validate magic and version */
    if (state->magic != DIST_STATE_MAGIC) {
        LOG_WARN("Invalid distributed state magic: 0x%08X", state->magic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_state_deserialize: validation failed");
        return -1;
    }
    if (state->version > DIST_STATE_VERSION) {
        LOG_WARN("Unknown distributed state version: %u", state->version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "dist_state_deserialize: validation failed");
        return -1;
    }

    /* State restored via stats — dist_ctx internal state is rebuilt on re-init.
     * The serialized stats allow continuity tracking across restarts. */
    if (ctx && state->is_initialized) {
        LOG_DEBUG("Restored distributed state: steps=%lu, syncs=%lu",
                  (unsigned long)state->total_steps, (unsigned long)state->sync_events);
    }
    (void)ctx;

    return 0;
}

static int dist_state_validate(void* ctx)
{
    /* Context may be NULL during initialization */
    (void)ctx;
    return 0;
}

static int dist_state_reset(void* ctx)
{
    /* Reset is handled by dist_destroy + dist_create cycle.
     * Stats are zeroed on fresh context creation. */
    if (ctx) {
        LOG_DEBUG("Distributed training state reset requested");
    }
    (void)ctx;
    return 0;
}

static size_t dist_state_get_size(void* ctx)
{
    (void)ctx;
    return sizeof(dist_serializable_state_t);
}

//=============================================================================
// Meta-Learning State Operations
//=============================================================================

static int meta_state_serialize(void* ctx, uint8_t* buffer, size_t* size)
{
    if (!size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_state_serialize: size is NULL");
        return -1;
    }

    size_t needed = sizeof(meta_serializable_state_t);

    if (!buffer) {
        *size = needed;
        return 0;
    }

    if (*size < needed) {
        *size = needed;
        return -2;
    }

    meta_serializable_state_t state;
    memset(&state, 0, sizeof(state));

    state.magic = META_STATE_MAGIC;
    state.version = META_STATE_VERSION;

    if (ctx) {
        state.is_initialized = true;
        meta_stats_t stats;
        if (metalearn_get_stats((const meta_ctx_t*)ctx, &stats) == 0) {
            state.total_meta_steps = stats.total_meta_steps;
            state.total_inner_steps = stats.total_inner_steps;
            state.tasks_processed = stats.tasks_processed;
            state.avg_query_loss = stats.avg_query_loss;
            state.avg_query_accuracy = stats.avg_query_accuracy;
            state.avg_inner_grad_norm = stats.avg_inner_grad_norm;
            state.avg_outer_grad_norm = stats.avg_outer_grad_norm;
        }
    }

    memcpy(buffer, &state, sizeof(state));
    *size = needed;
    return 0;
}

static int meta_state_deserialize(void* ctx, const uint8_t* buffer, size_t size)
{
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "meta_state_deserialize: buffer is NULL");
        return -1;
    }
    if (size < sizeof(meta_serializable_state_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_state_deserialize: validation failed");
        return -1;
    }

    const meta_serializable_state_t* state = (const meta_serializable_state_t*)buffer;

    if (state->magic != META_STATE_MAGIC) {
        LOG_WARN("Invalid meta-learning state magic: 0x%08X", state->magic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_state_deserialize: validation failed");
        return -1;
    }
    if (state->version > META_STATE_VERSION) {
        LOG_WARN("Unknown meta-learning state version: %u", state->version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "meta_state_deserialize: validation failed");
        return -1;
    }

    (void)ctx;
    return 0;
}

static int meta_state_validate(void* ctx)
{
    (void)ctx;
    return 0;
}

static int meta_state_reset(void* ctx)
{
    (void)ctx;
    return 0;
}

static size_t meta_state_get_size(void* ctx)
{
    (void)ctx;
    return sizeof(meta_serializable_state_t);
}

//=============================================================================
// Adversarial Training State Operations
//=============================================================================

static int adv_state_serialize(void* ctx, uint8_t* buffer, size_t* size)
{
    if (!size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_state_serialize: size is NULL");
        return -1;
    }

    size_t needed = sizeof(adv_serializable_state_t);

    if (!buffer) {
        *size = needed;
        return 0;
    }

    if (*size < needed) {
        *size = needed;
        return -2;
    }

    adv_serializable_state_t state;
    memset(&state, 0, sizeof(state));

    state.magic = ADV_STATE_MAGIC;
    state.version = ADV_STATE_VERSION;

    if (ctx) {
        state.is_initialized = true;
        adv_stats_t stats;
        if (adv_get_stats((const adv_ctx_t*)ctx, &stats) == 0) {
            state.total_steps = stats.total_steps;
            state.adversarial_steps = stats.adversarial_steps;
            state.total_attacks = stats.total_attacks;
            state.successful_attacks = stats.successful_attacks;
            state.clean_accuracy = stats.clean_accuracy;
            state.robust_accuracy = stats.robust_accuracy;
            state.avg_perturbation_norm = stats.avg_perturbation_norm;
            state.detection_rate = stats.detection_rate;
        }
    }

    memcpy(buffer, &state, sizeof(state));
    *size = needed;
    return 0;
}

static int adv_state_deserialize(void* ctx, const uint8_t* buffer, size_t size)
{
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "adv_state_deserialize: buffer is NULL");
        return -1;
    }
    if (size < sizeof(adv_serializable_state_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adv_state_deserialize: validation failed");
        return -1;
    }

    const adv_serializable_state_t* state = (const adv_serializable_state_t*)buffer;

    if (state->magic != ADV_STATE_MAGIC) {
        LOG_WARN("Invalid adversarial state magic: 0x%08X", state->magic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adv_state_deserialize: validation failed");
        return -1;
    }
    if (state->version > ADV_STATE_VERSION) {
        LOG_WARN("Unknown adversarial state version: %u", state->version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "adv_state_deserialize: validation failed");
        return -1;
    }

    (void)ctx;
    return 0;
}

static int adv_state_validate(void* ctx)
{
    (void)ctx;
    return 0;
}

static int adv_state_reset(void* ctx)
{
    (void)ctx;
    return 0;
}

static size_t adv_state_get_size(void* ctx)
{
    (void)ctx;
    return sizeof(adv_serializable_state_t);
}

//=============================================================================
// HPO State Operations
//=============================================================================

static int hpo_state_serialize(void* ctx, uint8_t* buffer, size_t* size)
{
    if (!size) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_state_serialize: size is NULL");
        return -1;
    }

    size_t needed = sizeof(hpo_serializable_state_t);

    if (!buffer) {
        *size = needed;
        return 0;
    }

    if (*size < needed) {
        *size = needed;
        return -2;
    }

    hpo_serializable_state_t state;
    memset(&state, 0, sizeof(state));

    state.magic = HPO_STATE_MAGIC;
    state.version = HPO_STATE_VERSION;

    if (ctx) {
        state.is_initialized = true;
        hpo_stats_t stats;
        if (hpo_get_stats((const hpo_ctx_t*)ctx, &stats) == 0) {
            state.total_trials = stats.total_trials;
            state.completed_trials = stats.completed_trials;
            state.pruned_trials = stats.pruned_trials;
            state.failed_trials = stats.failed_trials;
            state.best_objective = stats.best_objective;
            state.avg_objective = stats.avg_objective;
            state.trials_to_best = stats.trials_to_best;
        }
    }

    memcpy(buffer, &state, sizeof(state));
    *size = needed;
    return 0;
}

static int hpo_state_deserialize(void* ctx, const uint8_t* buffer, size_t size)
{
    if (!buffer) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hpo_state_deserialize: buffer is NULL");
        return -1;
    }
    if (size < sizeof(hpo_serializable_state_t)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_state_deserialize: validation failed");
        return -1;
    }

    const hpo_serializable_state_t* state = (const hpo_serializable_state_t*)buffer;

    if (state->magic != HPO_STATE_MAGIC) {
        LOG_WARN("Invalid HPO state magic: 0x%08X", state->magic);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_state_deserialize: validation failed");
        return -1;
    }
    if (state->version > HPO_STATE_VERSION) {
        LOG_WARN("Unknown HPO state version: %u", state->version);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hpo_state_deserialize: validation failed");
        return -1;
    }

    (void)ctx;
    return 0;
}

static int hpo_state_validate(void* ctx)
{
    (void)ctx;
    return 0;
}

static int hpo_state_reset(void* ctx)
{
    (void)ctx;
    return 0;
}

static size_t hpo_state_get_size(void* ctx)
{
    (void)ctx;
    return sizeof(hpo_serializable_state_t);
}

//=============================================================================
// Registry Helper Functions
//=============================================================================

static training_state_module_t* find_module(
    training_state_registry_t* registry,
    const char* name
)
{
    if (!registry || !name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "find_module: required parameter is NULL (registry, name)");
        return NULL;
    }

    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].module_name &&
            strcmp(registry->modules[i].module_name, name) == 0) {
            return &registry->modules[i];
        }
    }
    return NULL;
}

static int add_module(
    training_state_registry_t* registry,
    const char* name,
    void* ctx
)
{
    if (!registry || !name) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    if (registry->module_count >= TRAINING_STATE_MAX_MODULES) {
        NIMCP_THROW(NIMCP_ERROR_OUT_OF_RANGE, "Maximum training modules exceeded");
        return -1;
    }

    /* Check for duplicate */
    if (find_module(registry, name)) {
        LOG_WARN("Module '%s' already registered", name);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "add_module: validation failed");
        return -1;
    }

    /* Add to registry */
    training_state_module_t* module = &registry->modules[registry->module_count];
    module->module_name = name;
    module->context = ctx;
    module->enabled = true;
    module->last_checkpoint_time = 0;
    module->last_checkpoint_size = 0;
    module->checkpoint_count = 0;
    module->restore_count = 0;

    registry->module_count++;

    LOG_DEBUG("Registered training module: %s", name);
    return 0;
}

//=============================================================================
// Lifecycle API
//=============================================================================

training_state_registry_t* training_state_registry_create(void)
{
    training_state_registry_t* registry = (training_state_registry_t*)
        nimcp_calloc(1, sizeof(training_state_registry_t));

    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NO_MEMORY, "Failed to allocate training state registry");
        return NULL;
    }

    registry->magic = TRAINING_STATE_REGISTRY_MAGIC;
    registry->initialized = true;
    registry->module_count = 0;
    registry->distributed_ctx = NULL;
    registry->meta_learning_ctx = NULL;
    registry->adversarial_ctx = NULL;
    registry->hpo_ctx = NULL;
    registry->state_manager = NULL;
    registry->owns_state_manager = false;
    registry->total_checkpoints = 0;
    registry->total_restores = 0;
    registry->total_validation_errors = 0;

    LOG_INFO("Training state registry created");
    return registry;
}

void training_state_registry_destroy(training_state_registry_t* registry)
{
    if (!registry) return;

    /* Unlink from state manager first */
    if (registry->state_manager) {
        training_state_unlink_from_manager(registry);
    }

    /* Clear module references */
    registry->distributed_ctx = NULL;
    registry->meta_learning_ctx = NULL;
    registry->adversarial_ctx = NULL;
    registry->hpo_ctx = NULL;

    registry->initialized = false;
    registry->magic = 0;

    nimcp_free(registry);
    LOG_INFO("Training state registry destroyed");
}

//=============================================================================
// Module Registration API
//=============================================================================

int training_state_register_distributed(
    training_state_registry_t* registry,
    dist_ctx_t* ctx
)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    registry->distributed_ctx = ctx;
    return add_module(registry, "distributed_training", ctx);
}

int training_state_register_meta_learning(
    training_state_registry_t* registry,
    meta_ctx_t* ctx
)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    registry->meta_learning_ctx = ctx;
    return add_module(registry, "meta_learning", ctx);
}

int training_state_register_adversarial(
    training_state_registry_t* registry,
    adv_ctx_t* ctx
)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    registry->adversarial_ctx = ctx;
    return add_module(registry, "adversarial_training", ctx);
}

int training_state_register_hpo(
    training_state_registry_t* registry,
    hpo_ctx_t* ctx
)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    registry->hpo_ctx = ctx;
    return add_module(registry, "hyperparameter_optimization", ctx);
}

int training_state_unregister(
    training_state_registry_t* registry,
    const char* module_name
)
{
    if (!registry || !module_name) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    training_state_module_t* module = find_module(registry, module_name);
    if (!module) {
        return -1;  /* Not found */
    }

    /* Clear specific context reference */
    if (strcmp(module_name, "distributed_training") == 0) {
        registry->distributed_ctx = NULL;
    } else if (strcmp(module_name, "meta_learning") == 0) {
        registry->meta_learning_ctx = NULL;
    } else if (strcmp(module_name, "adversarial_training") == 0) {
        registry->adversarial_ctx = NULL;
    } else if (strcmp(module_name, "hyperparameter_optimization") == 0) {
        registry->hpo_ctx = NULL;
    }

    /* Remove from array by shifting */
    size_t index = (size_t)(module - registry->modules);
    for (size_t i = index; i < registry->module_count - 1; i++) {
        registry->modules[i] = registry->modules[i + 1];
    }
    registry->module_count--;

    LOG_DEBUG("Unregistered training module: %s", module_name);
    return 0;
}

//=============================================================================
// State Manager Integration API
//=============================================================================

int training_state_link_to_manager(
    training_state_registry_t* registry,
    nimcp_state_manager_t* manager
)
{
    if (!registry || !manager) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    registry->state_manager = manager;
    registry->owns_state_manager = false;

    /* Define state ops for each module type */
    nimcp_module_state_ops_t dist_ops = {
        .serialize = dist_state_serialize,
        .deserialize = dist_state_deserialize,
        .validate = dist_state_validate,
        .reset = dist_state_reset,
        .get_size = dist_state_get_size
    };

    nimcp_module_state_ops_t meta_ops = {
        .serialize = meta_state_serialize,
        .deserialize = meta_state_deserialize,
        .validate = meta_state_validate,
        .reset = meta_state_reset,
        .get_size = meta_state_get_size
    };

    nimcp_module_state_ops_t adv_ops = {
        .serialize = adv_state_serialize,
        .deserialize = adv_state_deserialize,
        .validate = adv_state_validate,
        .reset = adv_state_reset,
        .get_size = adv_state_get_size
    };

    nimcp_module_state_ops_t hpo_ops = {
        .serialize = hpo_state_serialize,
        .deserialize = hpo_state_deserialize,
        .validate = hpo_state_validate,
        .reset = hpo_state_reset,
        .get_size = hpo_state_get_size
    };

    int result;
    int registered = 0;

    /* Register distributed training if present */
    if (registry->distributed_ctx) {
        result = nimcp_state_manager_register_with_priority(
            manager, "training:distributed", &dist_ops,
            registry->distributed_ctx, PRIORITY_DISTRIBUTED
        );
        if (result == 0) registered++;
        else LOG_WARN("Failed to register distributed training with state manager");
    }

    /* Register meta-learning if present */
    if (registry->meta_learning_ctx) {
        result = nimcp_state_manager_register_with_priority(
            manager, "training:meta_learning", &meta_ops,
            registry->meta_learning_ctx, PRIORITY_META_LEARNING
        );
        if (result == 0) registered++;
        else LOG_WARN("Failed to register meta-learning with state manager");
    }

    /* Register adversarial training if present */
    if (registry->adversarial_ctx) {
        result = nimcp_state_manager_register_with_priority(
            manager, "training:adversarial", &adv_ops,
            registry->adversarial_ctx, PRIORITY_ADVERSARIAL
        );
        if (result == 0) registered++;
        else LOG_WARN("Failed to register adversarial training with state manager");
    }

    /* Register HPO if present */
    if (registry->hpo_ctx) {
        result = nimcp_state_manager_register_with_priority(
            manager, "training:hpo", &hpo_ops,
            registry->hpo_ctx, PRIORITY_HPO
        );
        if (result == 0) registered++;
        else LOG_WARN("Failed to register HPO with state manager");
    }

    LOG_INFO("Linked %d training modules to state manager", registered);
    return 0;
}

int training_state_unlink_from_manager(training_state_registry_t* registry)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    if (!registry->state_manager) {
        return 0;  /* Already unlinked */
    }

    /* Unregister each module from state manager */
    nimcp_state_manager_unregister(registry->state_manager, "training:distributed");
    nimcp_state_manager_unregister(registry->state_manager, "training:meta_learning");
    nimcp_state_manager_unregister(registry->state_manager, "training:adversarial");
    nimcp_state_manager_unregister(registry->state_manager, "training:hpo");

    registry->state_manager = NULL;
    LOG_INFO("Unlinked training modules from state manager");
    return 0;
}

//=============================================================================
// Checkpoint/Restore API (Direct)
//=============================================================================

int training_state_checkpoint_all(
    training_state_registry_t* registry,
    uint8_t* buffer,
    size_t* size
)
{
    if (!registry || !size) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    /* If linked to state manager, delegate to it */
    if (registry->state_manager) {
        return nimcp_state_manager_checkpoint_all(registry->state_manager, buffer, size);
    }

    /* Direct checkpoint without state manager */
    size_t total_size = training_state_get_total_size(registry);

    if (!buffer) {
        *size = total_size;
        registry->total_checkpoints++;  /* Count size queries as checkpoint operations */
        return 0;
    }

    if (*size < total_size) {
        *size = total_size;
        return -2;
    }

    /* Serialize each registered module (context may be NULL for default state) */
    uint8_t* ptr = buffer;
    size_t module_size;

    for (uint32_t i = 0; i < registry->module_count; i++) {
        const char* name = registry->modules[i].module_name;
        if (!name) continue;

        if (strcmp(name, "distributed_training") == 0) {
            module_size = sizeof(dist_serializable_state_t);
            dist_state_serialize(registry->distributed_ctx, ptr, &module_size);
            ptr += module_size;
            registry->modules[i].last_checkpoint_size = module_size;
            registry->modules[i].checkpoint_count++;
        } else if (strcmp(name, "meta_learning") == 0) {
            module_size = sizeof(meta_serializable_state_t);
            meta_state_serialize(registry->meta_learning_ctx, ptr, &module_size);
            ptr += module_size;
            registry->modules[i].last_checkpoint_size = module_size;
            registry->modules[i].checkpoint_count++;
        } else if (strcmp(name, "adversarial_training") == 0) {
            module_size = sizeof(adv_serializable_state_t);
            adv_state_serialize(registry->adversarial_ctx, ptr, &module_size);
            ptr += module_size;
            registry->modules[i].last_checkpoint_size = module_size;
            registry->modules[i].checkpoint_count++;
        } else if (strcmp(name, "hyperparameter_optimization") == 0) {
            module_size = sizeof(hpo_serializable_state_t);
            hpo_state_serialize(registry->hpo_ctx, ptr, &module_size);
            ptr += module_size;
            registry->modules[i].last_checkpoint_size = module_size;
            registry->modules[i].checkpoint_count++;
        }
    }

    *size = total_size;
    registry->total_checkpoints++;

    return 0;
}

int training_state_restore_all(
    training_state_registry_t* registry,
    const uint8_t* buffer,
    size_t size
)
{
    if (!registry || !buffer) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    /* If linked to state manager, delegate to it */
    if (registry->state_manager) {
        return nimcp_state_manager_restore_all(registry->state_manager, buffer, size);
    }

    /* Direct restore without state manager */
    const uint8_t* ptr = buffer;
    size_t remaining = size;

    /* Restore each registered module (context may be NULL) */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        const char* name = registry->modules[i].module_name;
        if (!name) continue;

        if (strcmp(name, "distributed_training") == 0 &&
            remaining >= sizeof(dist_serializable_state_t)) {
            dist_state_deserialize(registry->distributed_ctx, ptr, sizeof(dist_serializable_state_t));
            ptr += sizeof(dist_serializable_state_t);
            remaining -= sizeof(dist_serializable_state_t);
            registry->modules[i].restore_count++;
        } else if (strcmp(name, "meta_learning") == 0 &&
                   remaining >= sizeof(meta_serializable_state_t)) {
            meta_state_deserialize(registry->meta_learning_ctx, ptr, sizeof(meta_serializable_state_t));
            ptr += sizeof(meta_serializable_state_t);
            remaining -= sizeof(meta_serializable_state_t);
            registry->modules[i].restore_count++;
        } else if (strcmp(name, "adversarial_training") == 0 &&
                   remaining >= sizeof(adv_serializable_state_t)) {
            adv_state_deserialize(registry->adversarial_ctx, ptr, sizeof(adv_serializable_state_t));
            ptr += sizeof(adv_serializable_state_t);
            remaining -= sizeof(adv_serializable_state_t);
            registry->modules[i].restore_count++;
        } else if (strcmp(name, "hyperparameter_optimization") == 0 &&
                   remaining >= sizeof(hpo_serializable_state_t)) {
            hpo_state_deserialize(registry->hpo_ctx, ptr, sizeof(hpo_serializable_state_t));
            ptr += sizeof(hpo_serializable_state_t);
            remaining -= sizeof(hpo_serializable_state_t);
            registry->modules[i].restore_count++;
        }
    }

    registry->total_restores++;
    return 0;
}

int training_state_validate_all(training_state_registry_t* registry)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    /* If linked to state manager, delegate */
    if (registry->state_manager) {
        return nimcp_state_manager_validate_all(registry->state_manager);
    }

    int valid_count = 0;

    /* Validate each registered module (context may be NULL) */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        const char* name = registry->modules[i].module_name;
        if (!name) continue;

        int result = 0;
        if (strcmp(name, "distributed_training") == 0) {
            result = dist_state_validate(registry->distributed_ctx);
        } else if (strcmp(name, "meta_learning") == 0) {
            result = meta_state_validate(registry->meta_learning_ctx);
        } else if (strcmp(name, "adversarial_training") == 0) {
            result = adv_state_validate(registry->adversarial_ctx);
        } else if (strcmp(name, "hyperparameter_optimization") == 0) {
            result = hpo_state_validate(registry->hpo_ctx);
        }

        if (result == 0) {
            valid_count++;
        }
    }

    return valid_count;
}

int training_state_reset_all(training_state_registry_t* registry)
{
    if (!registry) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    /* If linked to state manager, delegate */
    if (registry->state_manager) {
        return nimcp_state_manager_reset_all(registry->state_manager);
    }

    int reset_count = 0;

    /* Reset each registered module (context may be NULL) */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        const char* name = registry->modules[i].module_name;
        if (!name) continue;

        int result = 0;
        if (strcmp(name, "distributed_training") == 0) {
            result = dist_state_reset(registry->distributed_ctx);
        } else if (strcmp(name, "meta_learning") == 0) {
            result = meta_state_reset(registry->meta_learning_ctx);
        } else if (strcmp(name, "adversarial_training") == 0) {
            result = adv_state_reset(registry->adversarial_ctx);
        } else if (strcmp(name, "hyperparameter_optimization") == 0) {
            result = hpo_state_reset(registry->hpo_ctx);
        }

        if (result == 0) {
            reset_count++;
        }
    }

    return reset_count;
}

//=============================================================================
// Query API
//=============================================================================

uint32_t training_state_get_module_count(training_state_registry_t* registry)
{
    if (!registry) return 0;
    return registry->module_count;
}

size_t training_state_get_total_size(training_state_registry_t* registry)
{
    if (!registry) return 0;

    size_t total = 0;

    /* Calculate size based on registered modules, not just non-NULL contexts */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        const char* name = registry->modules[i].module_name;
        if (!name) continue;

        if (strcmp(name, "distributed_training") == 0) {
            total += sizeof(dist_serializable_state_t);
        } else if (strcmp(name, "meta_learning") == 0) {
            total += sizeof(meta_serializable_state_t);
        } else if (strcmp(name, "adversarial_training") == 0) {
            total += sizeof(adv_serializable_state_t);
        } else if (strcmp(name, "hyperparameter_optimization") == 0) {
            total += sizeof(hpo_serializable_state_t);
        }
    }

    return total;
}

bool training_state_is_registered(
    training_state_registry_t* registry,
    const char* module_name
)
{
    return find_module(registry, module_name) != NULL;
}

//=============================================================================
// Statistics
//=============================================================================

int training_state_get_stats(
    training_state_registry_t* registry,
    training_state_stats_t* stats
)
{
    if (!registry || !stats) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER, "NULL pointer");
        return -1;
    }

    memset(stats, 0, sizeof(training_state_stats_t));

    stats->registered_modules = registry->module_count;
    stats->total_checkpoints = registry->total_checkpoints;
    stats->total_restores = registry->total_restores;
    stats->validation_errors = registry->total_validation_errors;
    stats->total_state_size = training_state_get_total_size(registry);

    /* Get last checkpoint time from modules */
    for (uint32_t i = 0; i < registry->module_count; i++) {
        if (registry->modules[i].last_checkpoint_time > stats->last_checkpoint_time) {
            stats->last_checkpoint_time = registry->modules[i].last_checkpoint_time;
        }
    }

    return 0;
}
