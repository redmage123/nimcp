#include <stddef.h>  /* for NULL */
//=============================================================================
// nimcp_training_callbacks.c - Training Event Callback System Implementation
//=============================================================================
/**
 * @file nimcp_training_callbacks.c
 * @brief Implementation of training callback system
 *
 * Phase TCB-1: Training Callbacks
 */

#include "middleware/training/nimcp_training_callbacks.h"
#include "nimcp.h"  /* nimcp_rubric_t, nimcp_brain_predict, nimcp_brain_rubric */
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"

#include "utils/error/nimcp_error_codes.h"
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/platform/nimcp_platform_mutex.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/logging/nimcp_logging.h"
#include "security/nimcp_security_integration.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/validation/nimcp_common.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

//=============================================================================
// Internal Constants
//=============================================================================

#define TCB_MAX_TOTAL_CALLBACKS 128
#define LOG_MODULE "training_callbacks"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(training_callbacks)

#define TCB_MAX_CHECKPOINT_PATH 512
#define TCB_MAX_NAME_LENGTH 64
#define TCB_EMA_ALPHA 0.1f
#define TCB_DIVERGENCE_WINDOW 10
#define TCB_MIN_STEPS_FOR_CONVERGENCE 100

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Internal callback entry
 */
typedef struct {
    tcb_callback_info_t info;
    uint32_t id;
    bool active;
} tcb_callback_entry_t;

/**
 * @brief Callback manager context
 */
struct tcb_context {
    /* Configuration */
    tcb_config_t config;

    /* Callbacks storage */
    tcb_callback_entry_t callbacks[TCB_MAX_TOTAL_CALLBACKS];
    uint32_t num_callbacks;
    uint32_t next_callback_id;

    /* Current metrics */
    tcb_metrics_t metrics;
    float loss_history[TCB_DIVERGENCE_WINDOW];
    uint32_t loss_history_idx;
    uint64_t step_start_time_ns;

    /* Early stopping state */
    float best_loss;
    uint32_t steps_without_improvement;
    bool early_stopping_triggered;

    /* Checkpoint state */
    char last_checkpoint_path[TCB_MAX_CHECKPOINT_PATH];
    uint64_t last_checkpoint_step;
    tcb_callback_fn checkpoint_handler;
    void* checkpoint_user_data;

    /* Statistics */
    tcb_stats_t stats;

    /* Thread safety */
    nimcp_platform_mutex_t mutex;

    /* Memory management */
    unified_mem_manager_t mem_mgr;
    bool owns_mem_mgr;

    /* Security */
    nimcp_sec_integration_t* security_ctx;
    uint32_t security_module_id;
    bool security_registered;
};

//=============================================================================
// Forward Declarations
//=============================================================================

static void tcb_init_metrics(tcb_metrics_t* metrics);
static void tcb_update_convergence_flags(tcb_context_t* ctx);
static tcb_action_t tcb_aggregate_actions(tcb_action_t a, tcb_action_t b);
static int tcb_compare_priority(const void* a, const void* b);
static void tcb_register_security(tcb_context_t* ctx);
static void tcb_unregister_security(tcb_context_t* ctx);

//=============================================================================
// Default Configuration
//=============================================================================

tcb_config_t tcb_config_default(void) {
    tcb_config_t config = {
        .enable_auto_checkpoint = false,
        .checkpoint_interval = TCB_DEFAULT_CHECKPOINT_INTERVAL,
        .checkpoint_dir = NULL,
        .max_checkpoints = 5,

        .enable_auto_logging = false,
        .log_interval = TCB_DEFAULT_LOG_INTERVAL,
        .log_to_file = false,
        .log_file_path = NULL,

        .enable_early_stopping = true,
        .patience = 10,
        .min_delta = 1e-6F,
        .divergence_threshold = 10.0F,

        .enable_async_callbacks = false,
        .async_queue_size = 256,
        .max_callback_time_us = TCB_MAX_CALLBACK_TIME_US,

        .use_memory_pool = true,
        .mem_strategy = UNIFIED_STRATEGY_AUTO,

        .security_ctx = NULL
    };
    return config;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

tcb_context_t* tcb_create(const tcb_config_t* config) {
    tcb_context_t* ctx = nimcp_calloc(1, sizeof(tcb_context_t));
    if (!ctx) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tcb_create: failed to allocate context");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        ctx->config = *config;
    } else {
        ctx->config = tcb_config_default();
    }

    /* Initialize mutex */
    if (nimcp_platform_mutex_init(&ctx->mutex, false) != 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "tcb_create: failed to initialize mutex");
        nimcp_free(ctx);
        return NULL;
    }

    /* Initialize metrics */
    tcb_init_metrics(&ctx->metrics);
    ctx->best_loss = INFINITY;
    ctx->next_callback_id = 1;

    /* Initialize loss history */
    for (int i = 0; i < TCB_DIVERGENCE_WINDOW; i++) {
        ctx->loss_history[i] = INFINITY;
    }

    /* Initialize memory manager if requested */
    if (ctx->config.use_memory_pool) {
        unified_mem_config_t mem_config = unified_mem_default_config();
        mem_config.default_strategy = ctx->config.mem_strategy;
        ctx->mem_mgr = unified_mem_create(&mem_config);
        ctx->owns_mem_mgr = (ctx->mem_mgr != NULL);
    }

    /* Security integration */
    ctx->security_ctx = ctx->config.security_ctx;
    if (ctx->security_ctx) {
        tcb_register_security(ctx);
    }

    return ctx;
}

void tcb_destroy(tcb_context_t* ctx) {
    if (!ctx) return;

    /* Unregister from security */
    if (ctx->security_registered) {
        tcb_unregister_security(ctx);
    }

    /* Destroy memory manager */
    if (ctx->owns_mem_mgr && ctx->mem_mgr) {
        unified_mem_destroy(ctx->mem_mgr);
    }

    /* Destroy mutex */
    nimcp_platform_mutex_destroy(&ctx->mutex);

    nimcp_free(ctx);
}

//=============================================================================
// Callback Registration
//=============================================================================

uint32_t tcb_register(tcb_context_t* ctx, const tcb_callback_info_t* info) {
    if (!ctx || !info || !info->callback) {
        return 0;
    }

    if (info->event_type >= TCB_EVENT_COUNT) {
        return 0;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    /* Check capacity */
    if (ctx->num_callbacks >= TCB_MAX_TOTAL_CALLBACKS) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return 0;
    }

    /* Count callbacks for this event type */
    uint32_t event_count = 0;
    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (ctx->callbacks[i].active &&
            ctx->callbacks[i].info.event_type == info->event_type) {
            event_count++;
        }
    }

    if (event_count >= TCB_MAX_CALLBACKS_PER_EVENT) {
        nimcp_platform_mutex_unlock(&ctx->mutex);
        return 0;
    }

    /* Find empty slot or append */
    uint32_t slot = ctx->num_callbacks;
    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (!ctx->callbacks[i].active) {
            slot = i;
            break;
        }
    }

    if (slot == ctx->num_callbacks) {
        ctx->num_callbacks++;
    }

    /* Store callback */
    ctx->callbacks[slot].info = *info;
    ctx->callbacks[slot].id = ctx->next_callback_id++;
    ctx->callbacks[slot].active = true;

    uint32_t id = ctx->callbacks[slot].id;

    nimcp_platform_mutex_unlock(&ctx->mutex);

    return id;
}

uint32_t tcb_register_simple(
    tcb_context_t* ctx,
    tcb_event_type_t event_type,
    tcb_callback_fn callback,
    void* user_data,
    const char* name)
{
    tcb_callback_info_t info = {
        .callback = callback,
        .user_data = user_data,
        .event_type = event_type,
        .mode = TCB_MODE_SYNC,
        .priority = TCB_PRIORITY_NORMAL,
        .name = name,
        .enabled = true
    };
    return tcb_register(ctx, &info);
}

nimcp_result_t tcb_unregister(tcb_context_t* ctx, uint32_t callback_id) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(callback_id != 0, NIMCP_ERROR_INVALID_PARAM, "callback_id is 0");

    nimcp_platform_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (ctx->callbacks[i].id == callback_id && ctx->callbacks[i].active) {
            ctx->callbacks[i].active = false;
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_NOT_FOUND;
}

nimcp_result_t tcb_set_enabled(tcb_context_t* ctx, uint32_t callback_id, bool enabled) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(callback_id != 0, NIMCP_ERROR_INVALID_PARAM, "callback_id is 0");

    nimcp_platform_mutex_lock(&ctx->mutex);

    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (ctx->callbacks[i].id == callback_id && ctx->callbacks[i].active) {
            ctx->callbacks[i].info.enabled = enabled;
            nimcp_platform_mutex_unlock(&ctx->mutex);
            return NIMCP_SUCCESS;
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return NIMCP_NOT_FOUND;
}

uint32_t tcb_unregister_all(tcb_context_t* ctx, tcb_event_type_t event_type) {
    if (!ctx) return 0;

    nimcp_platform_mutex_lock(&ctx->mutex);

    uint32_t removed = 0;
    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (ctx->callbacks[i].active) {
            if (event_type == TCB_EVENT_COUNT ||
                ctx->callbacks[i].info.event_type == event_type) {
                ctx->callbacks[i].active = false;
                removed++;
            }
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
    return removed;
}

//=============================================================================
// Event Firing
//=============================================================================

tcb_action_t tcb_fire(tcb_context_t* ctx, const tcb_event_t* event) {
    if (!ctx || !event) {
        return TCB_ACTION_CONTINUE;
    }

    if (event->event_type >= TCB_EVENT_COUNT) {
        return TCB_ACTION_CONTINUE;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    /* Collect matching callbacks */
    tcb_callback_entry_t* matches[TCB_MAX_CALLBACKS_PER_EVENT];
    uint32_t match_count = 0;

    for (uint32_t i = 0; i < ctx->num_callbacks && match_count < TCB_MAX_CALLBACKS_PER_EVENT; i++) {
        if (ctx->callbacks[i].active &&
            ctx->callbacks[i].info.enabled &&
            ctx->callbacks[i].info.event_type == event->event_type) {
            matches[match_count++] = &ctx->callbacks[i];
        }
    }

    /* Sort by priority (critical first) */
    if (match_count > 1) {
        qsort(matches, match_count, sizeof(tcb_callback_entry_t*), tcb_compare_priority);
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);

    /* Execute callbacks */
    tcb_action_t result = TCB_ACTION_CONTINUE;
    uint64_t start_time = nimcp_platform_time_monotonic_ms() * 1000000ULL;  /* Convert ms to ns */

    for (uint32_t i = 0; i < match_count; i++) {
        tcb_event_t cb_event = *event;
        cb_event.user_data = matches[i]->info.user_data;

        uint64_t cb_start = nimcp_platform_time_monotonic_ms() * 1000000ULL;
        tcb_action_t action = matches[i]->info.callback(&cb_event);
        uint64_t cb_elapsed = nimcp_platform_time_monotonic_ms() * 1000000ULL - cb_start;

        /* Update statistics */
        nimcp_platform_mutex_lock(&ctx->mutex);
        ctx->stats.total_callbacks_fired++;
        ctx->stats.callbacks_by_event[event->event_type]++;
        ctx->stats.total_execution_time_us += cb_elapsed / 1000;
        if (cb_elapsed / 1000 > ctx->stats.max_execution_time_us) {
            ctx->stats.max_execution_time_us = cb_elapsed / 1000;
        }

        /* Check for timeout */
        if (cb_elapsed / 1000 > ctx->config.max_callback_time_us) {
            ctx->stats.callbacks_timed_out++;
        }
        nimcp_platform_mutex_unlock(&ctx->mutex);

        /* Aggregate action */
        result = tcb_aggregate_actions(result, action);

        /* Stop early on critical actions */
        if (action == TCB_ACTION_STOP_TRAINING || action == TCB_ACTION_ROLLBACK) {
            break;
        }
    }

    /* Update stats */
    nimcp_platform_mutex_lock(&ctx->mutex);
    if (ctx->stats.total_callbacks_fired > 0) {
        ctx->stats.avg_execution_time_us =
            (float)ctx->stats.total_execution_time_us / ctx->stats.total_callbacks_fired;
    }

    /* Track special events */
    if (result == TCB_ACTION_STOP_TRAINING) {
        ctx->stats.early_stops_triggered++;
    }
    if (event->event_type == TCB_EVENT_DIVERGENCE) {
        ctx->stats.divergence_events++;
    }
    if (event->event_type == TCB_EVENT_CHECKPOINT) {
        ctx->stats.checkpoints_saved++;
    }
    if (result == TCB_ACTION_ROLLBACK) {
        ctx->stats.rollbacks_performed++;
    }
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return result;
}

tcb_action_t tcb_fire_event(
    tcb_context_t* ctx,
    tcb_event_type_t event_type,
    const tcb_metrics_t* metrics)
{
    if (!ctx) return TCB_ACTION_CONTINUE;

    tcb_event_t event = {
        .event_type = event_type,
        .metrics = metrics ? *metrics : ctx->metrics,
        .user_data = NULL,
        .checkpoint_path = ctx->last_checkpoint_path[0] ? ctx->last_checkpoint_path : NULL,
        .timestamp_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL
    };

    return tcb_fire(ctx, &event);
}

//=============================================================================
// Metrics Management
//=============================================================================

void tcb_update_metrics(
    tcb_context_t* ctx,
    float loss,
    float learning_rate,
    uint64_t step,
    float gradient_norm)
{
    if (!ctx) return;

    nimcp_platform_mutex_lock(&ctx->mutex);

    /* Calculate step time */
    uint64_t now = nimcp_platform_time_monotonic_ms() * 1000000ULL;
    if (ctx->step_start_time_ns > 0) {
        ctx->metrics.step_time_us = (now - ctx->step_start_time_ns) / 1000;
        ctx->metrics.total_time_us += ctx->metrics.step_time_us;
    }
    ctx->step_start_time_ns = now;

    /* Update loss metrics */
    float prev_loss = ctx->metrics.loss;
    ctx->metrics.loss = loss;
    ctx->metrics.loss_delta = loss - prev_loss;

    /* Exponential moving average - use passed step to check for first update */
    if (step == 0) {
        ctx->metrics.loss_ema = loss;
    } else {
        ctx->metrics.loss_ema = TCB_EMA_ALPHA * loss + (1.0F - TCB_EMA_ALPHA) * ctx->metrics.loss_ema;
    }

    /* Min/max loss */
    if (loss < ctx->metrics.min_loss) {
        ctx->metrics.min_loss = loss;
    }
    if (loss > ctx->metrics.max_loss && isfinite(loss)) {
        ctx->metrics.max_loss = loss;
    }

    /* Learning rate - use passed step to check for first update */
    if (step == 0) {
        ctx->metrics.initial_lr = learning_rate;
    }
    ctx->metrics.learning_rate = learning_rate;

    /* Gradient metrics */
    ctx->metrics.gradient_norm = gradient_norm;

    /* Step counter */
    ctx->metrics.step = step;

    /* Update loss history for divergence detection */
    ctx->loss_history[ctx->loss_history_idx] = loss;
    ctx->loss_history_idx = (ctx->loss_history_idx + 1) % TCB_DIVERGENCE_WINDOW;

    /* Calculate throughput */
    if (ctx->metrics.total_time_us > 0) {
        ctx->metrics.steps_per_second = (float)step / (ctx->metrics.total_time_us / 1e6F);
    }

    /* Update convergence flags */
    tcb_update_convergence_flags(ctx);

    /* Early stopping check */
    if (ctx->config.enable_early_stopping) {
        if (loss < ctx->best_loss - ctx->config.min_delta) {
            ctx->best_loss = loss;
            ctx->steps_without_improvement = 0;
        } else {
            ctx->steps_without_improvement++;
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);
}

nimcp_result_t tcb_get_metrics(const tcb_context_t* ctx, tcb_metrics_t* metrics) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(metrics != NULL, NIMCP_ERROR_INVALID_PARAM, "metrics is NULL");

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&ctx->mutex);
    *metrics = ctx->metrics;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&ctx->mutex);

    return NIMCP_SUCCESS;
}

//=============================================================================
// Checkpoint Management
//=============================================================================

nimcp_result_t tcb_checkpoint(tcb_context_t* ctx, const char* path) {
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->mutex);

    /* Generate path if not provided */
    if (path) {
        strncpy(ctx->last_checkpoint_path, path, TCB_MAX_CHECKPOINT_PATH - 1);
        ctx->last_checkpoint_path[TCB_MAX_CHECKPOINT_PATH - 1] = '\0';
    } else if (ctx->config.checkpoint_dir) {
        snprintf(ctx->last_checkpoint_path, TCB_MAX_CHECKPOINT_PATH,
                 "%s/checkpoint_step_%lu.nimcp",
                 ctx->config.checkpoint_dir, (unsigned long)ctx->metrics.step);
    } else {
        snprintf(ctx->last_checkpoint_path, TCB_MAX_CHECKPOINT_PATH,
                 "checkpoint_step_%lu.nimcp", (unsigned long)ctx->metrics.step);
    }

    ctx->last_checkpoint_step = ctx->metrics.step;

    nimcp_platform_mutex_unlock(&ctx->mutex);

    /* Fire checkpoint event */
    tcb_fire_event(ctx, TCB_EVENT_CHECKPOINT, NULL);

    /* Call checkpoint handler if set */
    if (ctx->checkpoint_handler) {
        tcb_event_t event = {
            .event_type = TCB_EVENT_CHECKPOINT,
            .metrics = ctx->metrics,
            .user_data = ctx->checkpoint_user_data,
            .checkpoint_path = ctx->last_checkpoint_path,
            .timestamp_ns = nimcp_platform_time_monotonic_ms() * 1000000ULL
        };
        ctx->checkpoint_handler(&event);
    }

    return NIMCP_SUCCESS;
}

nimcp_result_t tcb_set_checkpoint_handler(
    tcb_context_t* ctx,
    tcb_callback_fn callback,
    void* user_data)
{
    if (!ctx) return NIMCP_ERROR_INVALID_PARAM;

    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->checkpoint_handler = callback;
    ctx->checkpoint_user_data = user_data;
    nimcp_platform_mutex_unlock(&ctx->mutex);

    return NIMCP_SUCCESS;
}

const char* tcb_get_last_checkpoint(const tcb_context_t* ctx) {
    if (!ctx || ctx->last_checkpoint_path[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "tcb_get_last_checkpoint: ctx is NULL");
        return NULL;
    }
    return ctx->last_checkpoint_path;
}

//=============================================================================
// Early Stopping
//=============================================================================

bool tcb_should_stop(tcb_context_t* ctx, float current_loss) {
    if (!ctx || !ctx->config.enable_early_stopping) {
        return false;
    }

    nimcp_platform_mutex_lock(&ctx->mutex);

    bool should_stop = false;

    /* Check patience */
    if (ctx->steps_without_improvement >= ctx->config.patience) {
        should_stop = true;
        ctx->early_stopping_triggered = true;
    }

    /* Check divergence */
    if (isnan(current_loss) || isinf(current_loss)) {
        should_stop = true;
        ctx->early_stopping_triggered = true;
    }

    /* Check for loss explosion */
    if (ctx->metrics.step > TCB_MIN_STEPS_FOR_CONVERGENCE) {
        float avg_loss = 0;
        for (int i = 0; i < TCB_DIVERGENCE_WINDOW; i++) {
            avg_loss += ctx->loss_history[i];
        }
        avg_loss /= TCB_DIVERGENCE_WINDOW;

        if (current_loss > avg_loss * ctx->config.divergence_threshold) {
            should_stop = true;
            ctx->early_stopping_triggered = true;
            ctx->metrics.is_diverging = true;
        }
    }

    nimcp_platform_mutex_unlock(&ctx->mutex);

    return should_stop;
}

void tcb_reset_early_stopping(tcb_context_t* ctx) {
    if (!ctx) return;

    nimcp_platform_mutex_lock(&ctx->mutex);
    ctx->best_loss = INFINITY;
    ctx->steps_without_improvement = 0;
    ctx->early_stopping_triggered = false;
    nimcp_platform_mutex_unlock(&ctx->mutex);
}

uint32_t tcb_get_steps_without_improvement(const tcb_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->steps_without_improvement;
}

//=============================================================================
// Statistics
//=============================================================================

nimcp_result_t tcb_get_stats(const tcb_context_t* ctx, tcb_stats_t* stats) {
    NIMCP_CHECK_THROW(ctx != NULL, NIMCP_ERROR_INVALID_PARAM, "ctx is NULL");
    NIMCP_CHECK_THROW(stats != NULL, NIMCP_ERROR_INVALID_PARAM, "stats is NULL");

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&ctx->mutex);
    *stats = ctx->stats;
    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&ctx->mutex);

    return NIMCP_SUCCESS;
}

void tcb_reset_stats(tcb_context_t* ctx) {
    if (!ctx) return;

    nimcp_platform_mutex_lock(&ctx->mutex);
    memset(&ctx->stats, 0, sizeof(tcb_stats_t));
    nimcp_platform_mutex_unlock(&ctx->mutex);
}

uint32_t tcb_get_callback_count(const tcb_context_t* ctx, tcb_event_type_t event_type) {
    if (!ctx) return 0;

    nimcp_platform_mutex_lock((nimcp_platform_mutex_t*)&ctx->mutex);

    uint32_t count = 0;
    for (uint32_t i = 0; i < ctx->num_callbacks; i++) {
        if (ctx->callbacks[i].active) {
            if (event_type == TCB_EVENT_COUNT ||
                ctx->callbacks[i].info.event_type == event_type) {
                count++;
            }
        }
    }

    nimcp_platform_mutex_unlock((nimcp_platform_mutex_t*)&ctx->mutex);
    return count;
}

void tcb_print_status(const tcb_context_t* ctx) {
    if (!ctx) return;

    printf("\n=== Training Callback Manager Status ===\n");
    printf("Registered callbacks: %u\n", tcb_get_callback_count(ctx, TCB_EVENT_COUNT));
    printf("Total callbacks fired: %lu\n", (unsigned long)ctx->stats.total_callbacks_fired);
    printf("Avg execution time: %.2f us\n", ctx->stats.avg_execution_time_us);
    printf("Max execution time: %lu us\n", (unsigned long)ctx->stats.max_execution_time_us);
    printf("Callbacks timed out: %u\n", ctx->stats.callbacks_timed_out);
    printf("Early stops triggered: %u\n", ctx->stats.early_stops_triggered);
    printf("Divergence events: %u\n", ctx->stats.divergence_events);
    printf("Checkpoints saved: %u\n", ctx->stats.checkpoints_saved);

    printf("\nCallbacks by event type:\n");
    for (int i = 0; i < TCB_EVENT_COUNT; i++) {
        if (ctx->stats.callbacks_by_event[i] > 0) {
            printf("  %s: %lu\n", tcb_event_type_name(i),
                   (unsigned long)ctx->stats.callbacks_by_event[i]);
        }
    }

    printf("\nCurrent metrics:\n");
    printf("  Step: %lu\n", (unsigned long)ctx->metrics.step);
    printf("  Loss: %.6f (EMA: %.6f)\n", ctx->metrics.loss, ctx->metrics.loss_ema);
    printf("  Learning rate: %.6f\n", ctx->metrics.learning_rate);
    printf("  Gradient norm: %.6f\n", ctx->metrics.gradient_norm);
    printf("  Steps/sec: %.2f\n", ctx->metrics.steps_per_second);
    printf("  Converging: %s, Diverging: %s\n",
           ctx->metrics.is_converging ? "yes" : "no",
           ctx->metrics.is_diverging ? "yes" : "no");

    if (ctx->config.enable_early_stopping) {
        printf("\nEarly stopping:\n");
        printf("  Best loss: %.6f\n", ctx->best_loss);
        printf("  Steps without improvement: %u / %u\n",
               ctx->steps_without_improvement, ctx->config.patience);
    }

    printf("=========================================\n\n");
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* tcb_event_type_name(tcb_event_type_t event_type) {
    static const char* names[] = {
        "STEP_COMPLETE",
        "EPOCH_COMPLETE",
        "LOSS_COMPUTED",
        "WEIGHTS_UPDATED",
        "LR_CHANGED",
        "CONVERGENCE",
        "DIVERGENCE",
        "CHECKPOINT",
        "GRADIENT_CLIPPED",
        "BATCH_START",
        "BATCH_END",
        "VALIDATION"
    };

    if (event_type < TCB_EVENT_COUNT) {
        return names[event_type];
    }
    return "UNKNOWN";
}

const char* tcb_action_name(tcb_action_t action) {
    static const char* names[] = {
        "CONTINUE",
        "STOP_TRAINING",
        "SKIP_STEP",
        "ROLLBACK",
        "REDUCE_LR",
        "INCREASE_LR"
    };

    if (action <= TCB_ACTION_INCREASE_LR) {
        return names[action];
    }
    return "UNKNOWN";
}

nimcp_result_t tcb_validate_config(const tcb_config_t* config) {
    NIMCP_CHECK_THROW(config != NULL, NIMCP_ERROR_INVALID_PARAM, "config is NULL");
    NIMCP_CHECK_THROW(!(config->patience == 0 && config->enable_early_stopping),
        NIMCP_ERROR_INVALID_PARAM, "patience must be > 0 when early stopping is enabled");
    NIMCP_CHECK_THROW(config->divergence_threshold > 1.0F, NIMCP_ERROR_INVALID_PARAM,
        "divergence_threshold must be > 1.0");
    NIMCP_CHECK_THROW(config->max_callback_time_us > 0, NIMCP_ERROR_INVALID_PARAM,
        "max_callback_time_us must be > 0");

    return NIMCP_SUCCESS;
}

//=============================================================================
// Built-in Callbacks
//=============================================================================

tcb_action_t tcb_builtin_logger(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    const tcb_metrics_t* m = &event->metrics;

    printf("[Step %6lu] loss=%.6f lr=%.6f grad_norm=%.4f time=%.1fms\n",
           (unsigned long)m->step,
           m->loss,
           m->learning_rate,
           m->gradient_norm,
           m->step_time_us / 1000.0F);

    return TCB_ACTION_CONTINUE;
}

tcb_action_t tcb_builtin_early_stopper(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    const tcb_metrics_t* m = &event->metrics;

    /* Check for convergence (handled by manager's tcb_should_stop) */
    if (m->is_converging && m->loss < 1e-6F) {
        printf("[Early Stop] Training converged at step %lu (loss=%.8f)\n",
               (unsigned long)m->step, m->loss);
        return TCB_ACTION_STOP_TRAINING;
    }

    return TCB_ACTION_CONTINUE;
}

tcb_action_t tcb_builtin_divergence_detector(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    const tcb_metrics_t* m = &event->metrics;

    /* Check for NaN/Inf */
    if (isnan(m->loss) || isinf(m->loss)) {
        printf("[Divergence] NaN/Inf loss detected at step %lu\n",
               (unsigned long)m->step);
        return TCB_ACTION_STOP_TRAINING;
    }

    /* Check for divergence flag */
    if (m->is_diverging) {
        printf("[Divergence] Training diverging at step %lu (loss=%.4f)\n",
               (unsigned long)m->step, m->loss);
        return TCB_ACTION_REDUCE_LR;
    }

    return TCB_ACTION_CONTINUE;
}

tcb_action_t tcb_builtin_gradient_monitor(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    const tcb_metrics_t* m = &event->metrics;

    /* Exploding gradients */
    if (m->gradient_norm > 100.0F) {
        printf("[Gradient] Exploding gradients detected (norm=%.2f) at step %lu\n",
               m->gradient_norm, (unsigned long)m->step);
        return TCB_ACTION_REDUCE_LR;
    }

    /* Vanishing gradients */
    if (m->gradient_norm < 1e-7F && m->step > 100) {
        printf("[Gradient] Vanishing gradients detected (norm=%.2e) at step %lu\n",
               m->gradient_norm, (unsigned long)m->step);
        return TCB_ACTION_INCREASE_LR;
    }

    return TCB_ACTION_CONTINUE;
}

tcb_action_t tcb_builtin_progress_bar(const tcb_event_t* event) {
    if (!event) return TCB_ACTION_CONTINUE;

    const tcb_metrics_t* m = &event->metrics;

    /* Simple progress indicator */
    if (m->total_batches > 0) {
        uint32_t progress = (m->batch * 50) / m->total_batches;
        printf("\r[");
        for (uint32_t i = 0; i < 50; i++) {
            printf(i < progress ? "=" : (i == progress ? ">" : " "));
        }
        printf("] %u/%u loss=%.4f",
               m->batch, m->total_batches, m->loss);
        fflush(stdout);

        if (m->batch == m->total_batches) {
            printf("\n");
        }
    }

    return TCB_ACTION_CONTINUE;
}

tcb_action_t tcb_builtin_rubric_evaluator(const tcb_event_t* event) {
    if (!event || !event->user_data) return TCB_ACTION_CONTINUE;

    tcb_rubric_context_t* ctx = (tcb_rubric_context_t*)event->user_data;
    if (!ctx->brain) return TCB_ACTION_CONTINUE;

    /* Interval gating: only evaluate every N steps */
    if (ctx->interval > 0 && (event->metrics.step % ctx->interval) != 0) {
        return TCB_ACTION_CONTINUE;
    }

    /* Need validation features to run predict */
    if (!ctx->validation_features || ctx->num_features == 0) {
        return TCB_ACTION_CONTINUE;
    }

    char label[64];
    float confidence;
    nimcp_status_t rc = nimcp_brain_predict((nimcp_brain_t)ctx->brain, ctx->validation_features,
                                             ctx->num_features, label, &confidence);
    if (rc != NIMCP_OK) return TCB_ACTION_CONTINUE;

    nimcp_rubric_t rubric;
    rc = nimcp_brain_rubric((nimcp_brain_t)ctx->brain, &rubric);
    if (rc != 0) return TCB_ACTION_CONTINUE;

    /* Update context stats */
    ctx->eval_count++;
    ctx->last_score = rubric.overall_score;
    ctx->last_grade = rubric.grade;

    /* Threshold check */
    if (ctx->min_score > 0.0f && rubric.overall_score < ctx->min_score) {
        return TCB_ACTION_STOP_TRAINING;
    }

    return TCB_ACTION_CONTINUE;
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

static void tcb_init_metrics(tcb_metrics_t* metrics) {
    memset(metrics, 0, sizeof(tcb_metrics_t));
    metrics->min_loss = INFINITY;
    metrics->max_loss = -INFINITY;
}

static void tcb_update_convergence_flags(tcb_context_t* ctx) {
    /* Check if loss is consistently decreasing */
    bool decreasing = true;
    bool increasing = true;

    for (int i = 1; i < TCB_DIVERGENCE_WINDOW; i++) {
        int curr = (ctx->loss_history_idx - i + TCB_DIVERGENCE_WINDOW) % TCB_DIVERGENCE_WINDOW;
        int prev = (ctx->loss_history_idx - i - 1 + TCB_DIVERGENCE_WINDOW) % TCB_DIVERGENCE_WINDOW;

        if (ctx->loss_history[curr] >= ctx->loss_history[prev]) {
            decreasing = false;
        }
        if (ctx->loss_history[curr] <= ctx->loss_history[prev]) {
            increasing = false;
        }
    }

    ctx->metrics.is_converging = decreasing && ctx->metrics.step > TCB_MIN_STEPS_FOR_CONVERGENCE;
    ctx->metrics.is_diverging = increasing && ctx->metrics.loss > ctx->metrics.min_loss * 2.0F;

    /* Gradient health */
    ctx->metrics.gradients_exploding = ctx->metrics.gradient_norm > 100.0F;
    ctx->metrics.gradients_vanishing = ctx->metrics.gradient_norm < 1e-7F;
}

static tcb_action_t tcb_aggregate_actions(tcb_action_t a, tcb_action_t b) {
    /* Priority: STOP > ROLLBACK > REDUCE_LR > SKIP > INCREASE_LR > CONTINUE */
    if (a == TCB_ACTION_STOP_TRAINING || b == TCB_ACTION_STOP_TRAINING) {
        return TCB_ACTION_STOP_TRAINING;
    }
    if (a == TCB_ACTION_ROLLBACK || b == TCB_ACTION_ROLLBACK) {
        return TCB_ACTION_ROLLBACK;
    }
    if (a == TCB_ACTION_REDUCE_LR || b == TCB_ACTION_REDUCE_LR) {
        return TCB_ACTION_REDUCE_LR;
    }
    if (a == TCB_ACTION_SKIP_STEP || b == TCB_ACTION_SKIP_STEP) {
        return TCB_ACTION_SKIP_STEP;
    }
    if (a == TCB_ACTION_INCREASE_LR || b == TCB_ACTION_INCREASE_LR) {
        return TCB_ACTION_INCREASE_LR;
    }
    return TCB_ACTION_CONTINUE;
}

static int tcb_compare_priority(const void* a, const void* b) {
    const tcb_callback_entry_t* ca = *(const tcb_callback_entry_t**)a;
    const tcb_callback_entry_t* cb = *(const tcb_callback_entry_t**)b;
    /* Higher priority first (descending order) */
    return (int)cb->info.priority - (int)ca->info.priority;
}

static void tcb_register_security(tcb_context_t* ctx) {
    if (!ctx || !ctx->security_ctx) return;

    nimcp_result_t result = nimcp_sec_register_module(
        ctx->security_ctx,
        TCB_SECURITY_MODULE_NAME,
        NIMCP_SEC_CAT_MIDDLEWARE,
        &ctx->security_module_id
    );

    if (result == NIMCP_SUCCESS) {
        ctx->security_registered = true;
    }
}

static void tcb_unregister_security(tcb_context_t* ctx) {
    if (!ctx || !ctx->security_ctx || !ctx->security_registered) return;

    nimcp_sec_unregister_module(ctx->security_ctx, ctx->security_module_id);
    ctx->security_registered = false;
}
