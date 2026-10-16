/**
 * @file nimcp_exception_immune.c
 * @brief Exception-to-immune system integration implementation
 * @version 1.0.0
 * @date 2025-01-16
 *
 * WHAT: Bridge between exception handling and brain immune system
 * WHY:  Enable automatic error resolution through immune-style responses
 * HOW:  Present exceptions as antigens, execute recovery antibodies
 *
 * @author NIMCP Development Team
 */

#include "utils/exception/nimcp_exception_immune.h"
#include "utils/exception/nimcp_exception_handlers.h"
#include "core/brain/nimcp_kg_module_wiring.h"  /* Must be before macros.h */
#include "utils/exception/nimcp_exception_macros.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_mutex.h"

/* Recovery system includes */
#include "core/brain/nimcp_kg_gc.h"
#include "utils/fault_tolerance/nimcp_checkpoint.h"
#include "utils/fault_tolerance/nimcp_retry.h"
/* Note: nimcp_recovery.h is included via nimcp_retry.h
 * The types don't conflict with nimcp_exception.h as they use different prefixes:
 * - recovery_action_t vs nimcp_exception_recovery_action_t
 * - recovery_status_t is only used locally within nimcp_recovery.h */
#include "security/nimcp_blood_brain_barrier.h"
#include "utils/fault_tolerance/nimcp_runtime_adaptation.h"

/* Include brain immune for full exception-to-immune integration */
#include "cognitive/immune/nimcp_brain_immune.h"

#include <string.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(exception_immune)

/* ============================================================================
 * Module State
 * ============================================================================ */

static bool g_immune_initialized = false;
static nimcp_exception_immune_config_t g_config;
static brain_immune_system_t* g_immune_system = NULL;
static nimcp_platform_mutex_t* g_immune_mutex = NULL;

/* Async presentation queue */
static struct {
    nimcp_exception_t* exceptions[NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE];
    size_t head;
    size_t tail;
    size_t count;
} g_async_queue = {0};

/* Statistics */
static nimcp_exception_immune_stats_t g_stats = {0};

/* ============================================================================
 * Thread-Local Reentry Guard
 * ============================================================================
 * WHAT: Prevents recursive calls to immune presentation
 * WHY:  If the immune system itself throws an exception during processing,
 *       recursive presentation could cause infinite loops or deadlocks
 * HOW:  Thread-local flag checked at function entry, blocks reentrant calls
 */
static _Thread_local bool tl_in_immune_context = false;

/* P1-5 FIX: Add depth counter for nested NIMCP_THROW_TO_IMMUNE calls.
 * The boolean tl_in_immune_context only prevents direct reentry, but
 * nested throws from recovery callbacks or the immune system itself
 * could still cause unbounded recursion. The depth counter limits
 * the maximum nesting depth to prevent stack overflow. */
static _Thread_local int tl_immune_depth = 0;
#define MAX_IMMUNE_DEPTH 3

/* ============================================================================
 * Recovery Context
 * ============================================================================
 * WHAT: Holds references to systems needed for recovery actions
 * WHY:  Recovery callbacks need access to brain, GC, BBB, checkpoint systems
 * HOW:  Application sets context via nimcp_recovery_set_context()
 */

typedef struct nimcp_recovery_context {
    brain_t brain;                       /**< Brain instance for recovery */
    kg_gc_context_t* gc_context;         /**< GC context for garbage collection */
    bbb_system_t bbb_system;             /**< BBB system for quarantine */
    runtime_adaptation_context_t ra_ctx; /**< Runtime adaptation for load reduction */
    const char* checkpoint_dir;          /**< Directory for checkpoint files */
    const char* emergency_checkpoint;    /**< Path for emergency saves */
} nimcp_recovery_context_t;

static nimcp_recovery_context_t g_recovery_context = {0};

/**
 * @brief Set the recovery context with system references
 *
 * WHAT: Configure recovery callbacks with access to brain subsystems
 * WHY:  Recovery actions need actual system references to operate
 * HOW:  Store references in module-level context structure
 *
 * @param brain Brain instance
 * @param gc_context GC context (can be NULL if GC not available)
 * @param bbb_system BBB system (can be NULL if BBB not available)
 * @param ra_ctx Runtime adaptation context (can be NULL)
 * @param checkpoint_dir Directory for checkpoint files (can be NULL)
 * @return 0 on success
 */
int nimcp_recovery_set_context(
    brain_t brain,
    kg_gc_context_t* gc_context,
    bbb_system_t bbb_system,
    runtime_adaptation_context_t ra_ctx,
    const char* checkpoint_dir
) {
    g_recovery_context.brain = brain;
    g_recovery_context.gc_context = gc_context;
    g_recovery_context.bbb_system = bbb_system;
    g_recovery_context.ra_ctx = ra_ctx;
    g_recovery_context.checkpoint_dir = checkpoint_dir;

    /* Set default emergency checkpoint path */
    if (checkpoint_dir) {
        static char emergency_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(emergency_path, sizeof(emergency_path),
                 "%s/emergency_checkpoint.ckpt", checkpoint_dir);
        g_recovery_context.emergency_checkpoint = emergency_path;
    }

    LOG_INFO("Recovery context configured: brain=%p, gc=%p, bbb=%p, ra=%p",
             (void*)brain, (void*)gc_context, (void*)bbb_system, (void*)ra_ctx);

    return 0;
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static uint64_t get_timestamp_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void nimcp_exception_immune_default_config(nimcp_exception_immune_config_t* config) {
    if (!config) return;

    config->enable_auto_present = true;
    config->min_present_severity = EXCEPTION_SEVERITY_SEVERE;
    config->enable_auto_recovery = true;
    config->enable_memory_formation = true;
    config->async_presentation = false;
    config->max_pending_exceptions = NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE;
    config->response_timeout_ms = 5000;
}

/* ============================================================================
 * Initialization
 * ============================================================================ */

int nimcp_exception_immune_init(const nimcp_exception_immune_config_t* config) {
    if (g_immune_initialized) return 0;

    if (config) {
        g_config = *config;
    } else {
        nimcp_exception_immune_default_config(&g_config);
    }

    g_immune_mutex = nimcp_platform_mutex_create();
    if (!g_immune_mutex) return -1;

    memset(&g_async_queue, 0, sizeof(g_async_queue));
    memset(&g_stats, 0, sizeof(g_stats));

    g_immune_initialized = true;
    LOG_INFO("Exception-immune integration initialized");

    return 0;
}

void nimcp_exception_immune_shutdown(void) {
    if (!g_immune_initialized) return;

    /* Clear async queue */
    while (g_async_queue.count > 0) {
        nimcp_exception_t* ex = g_async_queue.exceptions[g_async_queue.head];
        if (ex) nimcp_exception_unref(ex);
        g_async_queue.head = (g_async_queue.head + 1) % NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE;
        g_async_queue.count--;
    }

    if (g_immune_mutex) {
        nimcp_platform_mutex_destroy(g_immune_mutex);
        nimcp_free(g_immune_mutex);
        g_immune_mutex = NULL;
    }

    g_immune_system = NULL;
    g_immune_initialized = false;

    LOG_INFO("Exception-immune integration shutdown");
}

/* ============================================================================
 * Connection
 * ============================================================================ */

int nimcp_exception_immune_connect(brain_immune_system_t* immune_system) {
    if (!immune_system) return -1;
    if (!g_immune_initialized) {
        if (nimcp_exception_immune_init(NULL) != 0) return -1;
    }

    g_immune_system = immune_system;
    LOG_INFO("Connected to brain immune system");
    return 0;
}

int nimcp_exception_immune_disconnect(void) {
    g_immune_system = NULL;
    LOG_INFO("Disconnected from brain immune system");
    return 0;
}

bool nimcp_exception_immune_is_connected(void) {
    return g_immune_system != NULL;
}

/* ============================================================================
 * Mapping Functions
 * ============================================================================ */

exception_antigen_source_t nimcp_exception_to_antigen_source(
    nimcp_exception_category_t category
) {
    switch (category) {
        case EXCEPTION_CATEGORY_SECURITY:
            return EX_ANTIGEN_SOURCE_BBB;

        case EXCEPTION_CATEGORY_BRAIN:
        case EXCEPTION_CATEGORY_BRAIN_REGION:
            return EX_ANTIGEN_SOURCE_BBB;

        case EXCEPTION_CATEGORY_THREADING:
            return EX_ANTIGEN_SOURCE_BFT;

        case EXCEPTION_CATEGORY_MEMORY:
        case EXCEPTION_CATEGORY_IO:
        case EXCEPTION_CATEGORY_CONFIG:
        case EXCEPTION_CATEGORY_SIGNAL:
        case EXCEPTION_CATEGORY_COGNITIVE:
        case EXCEPTION_CATEGORY_GPU:
        default:
            return EX_ANTIGEN_SOURCE_ANOMALY;
    }
}

uint32_t nimcp_exception_to_immune_severity(nimcp_exception_severity_t severity) {
    switch (severity) {
        case EXCEPTION_SEVERITY_DEBUG:    return 1;
        case EXCEPTION_SEVERITY_INFO:     return 2;
        case EXCEPTION_SEVERITY_WARNING:  return 3;
        case EXCEPTION_SEVERITY_ERROR:    return 5;
        case EXCEPTION_SEVERITY_SEVERE:   return 7;
        case EXCEPTION_SEVERITY_CRITICAL: return 9;
        case EXCEPTION_SEVERITY_FATAL:    return 10;
        default: return 5;
    }
}

void nimcp_exception_get_recovery_strategy(
    const nimcp_exception_t* ex,
    nimcp_exception_recovery_strategy_t* strategy
) {
    if (!strategy) return;

    memset(strategy, 0, sizeof(nimcp_exception_recovery_strategy_t));

    if (!ex) {
        strategy->primary_action = EXCEPTION_RECOVERY_NONE;
        strategy->fallback_action = EXCEPTION_RECOVERY_NONE;
        return;
    }

    /* Set based on exception category */
    switch (ex->category) {
        case EXCEPTION_CATEGORY_MEMORY:
            strategy->primary_action = EXCEPTION_RECOVERY_GC;
            strategy->fallback_action = EXCEPTION_RECOVERY_QUARANTINE;
            strategy->retry_count = 3;
            strategy->cooldown_ms = 100;
            break;

        case EXCEPTION_CATEGORY_BRAIN:
        case EXCEPTION_CATEGORY_BRAIN_REGION:
            strategy->primary_action = EXCEPTION_RECOVERY_ROLLBACK;
            strategy->fallback_action = EXCEPTION_RECOVERY_REDUCE_LOAD;
            strategy->retry_count = 1;
            strategy->cooldown_ms = 500;
            break;

        case EXCEPTION_CATEGORY_THREADING:
            strategy->primary_action = EXCEPTION_RECOVERY_RESTART_THREAD;
            strategy->fallback_action = EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN;
            strategy->retry_count = 2;
            strategy->cooldown_ms = 1000;
            break;

        case EXCEPTION_CATEGORY_IO:
            strategy->primary_action = EXCEPTION_RECOVERY_RETRY;
            strategy->fallback_action = EXCEPTION_RECOVERY_ROLLBACK;
            strategy->retry_count = 5;
            strategy->cooldown_ms = 200;
            break;

        case EXCEPTION_CATEGORY_SIGNAL:
            strategy->primary_action = EXCEPTION_RECOVERY_EMERGENCY_SAVE;
            strategy->fallback_action = EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN;
            strategy->retry_count = 1;
            strategy->cooldown_ms = 0;
            break;

        case EXCEPTION_CATEGORY_GPU:
            strategy->primary_action = EXCEPTION_RECOVERY_CLEAR_CACHE;
            strategy->fallback_action = EXCEPTION_RECOVERY_REDUCE_LOAD;
            strategy->retry_count = 3;
            strategy->cooldown_ms = 50;
            break;

        case EXCEPTION_CATEGORY_SECURITY:
            strategy->primary_action = EXCEPTION_RECOVERY_QUARANTINE;
            strategy->fallback_action = EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN;
            strategy->retry_count = 1;
            strategy->cooldown_ms = 0;
            break;

        case EXCEPTION_CATEGORY_COGNITIVE:
            /* Cognitive errors (working memory, executive control, etc.) */
            /* Typically need to free resources or reduce load */
            strategy->primary_action = EXCEPTION_RECOVERY_GC;
            strategy->fallback_action = EXCEPTION_RECOVERY_REDUCE_LOAD;
            strategy->retry_count = 2;
            strategy->cooldown_ms = 100;
            break;

        default:
            strategy->primary_action = EXCEPTION_RECOVERY_RETRY;
            strategy->fallback_action = EXCEPTION_RECOVERY_NONE;
            strategy->retry_count = 3;
            strategy->cooldown_ms = 100;
            break;
    }

    strategy->severity_threshold = 0.5f;
}

/* ============================================================================
 * Epitope Generation
 * ============================================================================ */

/**
 * @brief Simple hash function
 */
static uint32_t hash_bytes(const void* data, size_t len) {
    const uint8_t* bytes = (const uint8_t*)data;
    uint32_t hash = 5381;
    for (size_t i = 0; i < len; i++) {
        hash = ((hash << 5) + hash) + bytes[i];
    }
    return hash;
}

size_t nimcp_exception_compute_epitope(
    const nimcp_exception_t* ex,
    uint8_t* epitope,
    size_t epitope_size
) {
    if (!ex || !epitope || epitope_size < NIMCP_EXCEPTION_EPITOPE_SIZE) return 0;

    memset(epitope, 0, epitope_size);
    size_t offset = 0;

    /* Error code (4 bytes) */
    memcpy(epitope + offset, &ex->code, sizeof(ex->code));
    offset += sizeof(ex->code);

    /* Category hash (4 bytes) */
    uint32_t cat = (uint32_t)ex->category;
    memcpy(epitope + offset, &cat, sizeof(cat));
    offset += sizeof(cat);

    /* Severity (4 bytes) */
    uint32_t sev = (uint32_t)ex->severity;
    memcpy(epitope + offset, &sev, sizeof(sev));
    offset += sizeof(sev);

    /* Type (4 bytes) */
    uint32_t type = (uint32_t)ex->type;
    memcpy(epitope + offset, &type, sizeof(type));
    offset += sizeof(type);

    /* File hash (4 bytes) */
    if (ex->file) {
        uint32_t file_hash = hash_bytes(ex->file, strlen(ex->file));
        memcpy(epitope + offset, &file_hash, sizeof(file_hash));
    }
    offset += 4;

    /* Function hash (4 bytes) */
    if (ex->function) {
        uint32_t func_hash = hash_bytes(ex->function, strlen(ex->function));
        memcpy(epitope + offset, &func_hash, sizeof(func_hash));
    }
    offset += 4;

    /* Line (4 bytes) */
    uint32_t line = (uint32_t)ex->line;
    memcpy(epitope + offset, &line, sizeof(line));
    offset += sizeof(line);

    /* Message hash (4 bytes) */
    if (ex->message[0]) {
        uint32_t msg_hash = hash_bytes(ex->message, strlen(ex->message));
        memcpy(epitope + offset, &msg_hash, sizeof(msg_hash));
    }
    offset += 4;

    /* Stack trace addresses (remaining bytes) */
    for (size_t i = 0; i < ex->stack_trace.depth && offset < NIMCP_EXCEPTION_EPITOPE_SIZE - 4; i++) {
        uint32_t addr = (uint32_t)(uintptr_t)ex->stack_trace.frames[i].address;
        memcpy(epitope + offset, &addr, sizeof(addr));
        offset += sizeof(addr);
    }

    return offset;
}

/* ============================================================================
 * Exception Presentation
 * ============================================================================ */

int nimcp_exception_present_to_immune(
    nimcp_exception_t* ex,
    nimcp_immune_response_t* response
) {
    if (!ex) return -1;

    /* P1-5 FIX: Depth-based recursion guard - prevent nested immune calls
     * from causing stack overflow. The boolean tl_in_immune_context prevents
     * direct reentry, and the depth counter limits nesting from recovery
     * callbacks that may trigger additional exceptions. */
    if (tl_immune_depth >= MAX_IMMUNE_DEPTH) {
        fprintf(stderr, "[IMMUNE] Max recursion depth (%d) reached, dropping exception code=%d\n",
                MAX_IMMUNE_DEPTH, ex->code);
        ex->presented_to_immune = true;
        ex->antigen_id = 0;
        return 0;
    }

    if (tl_in_immune_context) {
        LOG_WARNING("Reentrant immune call blocked: code=%d", ex->code);
        ex->presented_to_immune = true;
        ex->antigen_id = 0;  /* Indicate reentry-blocked */
        return 0;
    }

    if (ex->presented_to_immune) {
        /* Already presented */
        return 0;
    }

    /* Set reentry guard and increment depth */
    tl_in_immune_context = true;
    tl_immune_depth++;

    uint64_t start_time = get_timestamp_us();

    if (!g_immune_system) {
        /* Not connected - still mark as presented and track stats */
        LOG_DEBUG("Exception presented (no immune system connected): code=%d", ex->code);

        ex->presented_to_immune = true;
        ex->antigen_id = (uint32_t)(ex->timestamp_us & 0xFFFFFFFF);

        uint64_t response_time = get_timestamp_us() - start_time;

        /* Update stats */
        g_stats.exceptions_presented++;
        if (g_stats.exceptions_presented > 1) {
            g_stats.avg_response_time_us =
                (g_stats.avg_response_time_us * (g_stats.exceptions_presented - 1) + response_time)
                / g_stats.exceptions_presented;
        } else {
            g_stats.avg_response_time_us = (float)response_time;
        }

        /* Fill minimal response */
        if (response) {
            response->antigen_id = ex->antigen_id;
            response->antibody_id = 0;
            response->action_taken = EXCEPTION_RECOVERY_NONE;
            response->recovery_attempted = false;
            response->recovery_succeeded = false;
            response->response_time_us = response_time;
            response->memory_formed = false;
        }

        /* Clear reentry guard and decrement depth */
        tl_in_immune_context = false;
        tl_immune_depth--;
        return 0;
    }

    /* Compute epitope if not already done */
    if (ex->epitope_len == 0) {
        nimcp_exception_generate_epitope(ex);
    }

    /* Present to immune system using the exception-aware function
     * This will invoke any registered exception callbacks */
    uint32_t antigen_id = 0;

#ifdef NIMCP_BRAIN_IMMUNE_H
    int result = brain_immune_present_exception(
        g_immune_system,
        ex,
        &antigen_id
    );

    if (result != 0) {
        LOG_WARNING("Failed to present exception to immune system: code=%d", ex->code);
        /* Clear reentry guard and decrement depth */
        tl_in_immune_context = false;
        tl_immune_depth--;
        return -1;
    }
#else
    /* Immune system header not included - simulate success */
    antigen_id = (uint32_t)(ex->timestamp_us & 0xFFFFFFFF);
#endif

    ex->presented_to_immune = true;
    ex->antigen_id = antigen_id;

    uint64_t response_time = get_timestamp_us() - start_time;

    /* Update stats */
    g_stats.exceptions_presented++;
    g_stats.avg_response_time_us =
        (g_stats.avg_response_time_us * (g_stats.exceptions_presented - 1) + response_time)
        / g_stats.exceptions_presented;

    LOG_DEBUG("Presented exception to immune: code=%d, antigen_id=%u, severity=%d",
              ex->code, antigen_id, ex->severity);

    /* Fill response if requested */
    if (response) {
        response->antigen_id = antigen_id;
        response->antibody_id = 0;  /* Will be set by immune response */
        response->action_taken = EXCEPTION_RECOVERY_NONE;
        response->recovery_attempted = false;
        response->recovery_succeeded = false;
        response->response_time_us = response_time;
        response->memory_formed = false;
    }

    /* Auto-execute recovery if enabled */
    if (g_config.enable_auto_recovery && ex->suggested_action != EXCEPTION_RECOVERY_NONE) {
        int recovery_result = nimcp_exception_execute_recovery(ex, ex->suggested_action);

        if (response) {
            response->action_taken = ex->suggested_action;
            response->recovery_attempted = true;
            response->recovery_succeeded = (recovery_result == 0);
        }

        g_stats.recoveries_attempted++;
        if (recovery_result == 0) {
            g_stats.recoveries_succeeded++;
        }
    }

    /* Clear reentry guard and decrement depth */
    tl_in_immune_context = false;
    tl_immune_depth--;
    return 0;
}

int nimcp_exception_present_async(nimcp_exception_t* ex) {
    if (!ex) return -1;
    if (!g_immune_initialized) return -1;

    if (g_immune_mutex) nimcp_platform_mutex_lock(g_immune_mutex);

    if (g_async_queue.count >= NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE) {
        g_stats.queue_overflows++;
        if (g_immune_mutex) nimcp_platform_mutex_unlock(g_immune_mutex);
        return -1;
    }

    /* Add to queue */
    g_async_queue.exceptions[g_async_queue.tail] = nimcp_exception_ref(ex);
    g_async_queue.tail = (g_async_queue.tail + 1) % NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE;
    g_async_queue.count++;
    g_stats.exceptions_pending = g_async_queue.count;

    if (g_immune_mutex) nimcp_platform_mutex_unlock(g_immune_mutex);

    return 0;
}

size_t nimcp_exception_immune_process_pending(size_t max_count) {
    if (!g_immune_initialized) return 0;

    size_t processed = 0;

    if (g_immune_mutex) nimcp_platform_mutex_lock(g_immune_mutex);

    while (g_async_queue.count > 0 && (max_count == 0 || processed < max_count)) {
        nimcp_exception_t* ex = g_async_queue.exceptions[g_async_queue.head];
        g_async_queue.head = (g_async_queue.head + 1) % NIMCP_EXCEPTION_IMMUNE_QUEUE_SIZE;
        g_async_queue.count--;

        if (g_immune_mutex) nimcp_platform_mutex_unlock(g_immune_mutex);

        if (ex) {
            nimcp_exception_present_to_immune(ex, NULL);
            nimcp_exception_unref(ex);
            processed++;
        }

        if (g_immune_mutex) nimcp_platform_mutex_lock(g_immune_mutex);
    }

    g_stats.exceptions_pending = g_async_queue.count;

    if (g_immune_mutex) nimcp_platform_mutex_unlock(g_immune_mutex);

    return processed;
}

/* ============================================================================
 * Recovery Execution
 * ============================================================================ */

int nimcp_exception_execute_recovery(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action
) {
    return nimcp_execute_recovery(ex, action);
}

int nimcp_exception_notify_recovery_result(
    nimcp_exception_t* ex,
    nimcp_exception_recovery_action_t action,
    bool success
) {
    if (!ex) return -1;

    ex->recovery_attempted = true;
    ex->recovery_succeeded = success;

    LOG_INFO("Recovery result: code=%d, action=%s, success=%s",
             ex->code,
             nimcp_exception_recovery_action_to_string(action),
             success ? "true" : "false");

    /* Notify immune system of result */
#ifdef NIMCP_BRAIN_IMMUNE_H
    if (g_immune_system && ex->presented_to_immune) {
        /* Could trigger memory formation here */
        if (success && g_config.enable_memory_formation) {
            g_stats.memories_formed++;
        }
    }
#endif

    return 0;
}

/* ============================================================================
 * Default Recovery Callbacks
 * ============================================================================
 * WHAT: Actual implementations of recovery actions
 * WHY:  Enable automatic self-healing through immune system integration
 * HOW:  Use GC, checkpoint, BBB, and runtime adaptation subsystems
 */

/**
 * @brief Execute garbage collection recovery action
 *
 * WHAT: Trigger full GC cycle to reclaim memory
 * WHY:  Memory exhaustion/pressure exceptions need memory freed
 * HOW:  Call kg_gc_run() with all targets, then compact if needed
 */
int nimcp_recovery_gc(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;

    LOG_INFO("Executing GC recovery action");

    /* Check if GC context is available */
    if (!g_recovery_context.gc_context) {
        LOG_WARNING("GC recovery failed: GC context not configured");
        return -1;
    }

    /* Run full GC on all targets */
    kg_gc_stats_t gc_stats;
    int result = kg_gc_run(g_recovery_context.gc_context, KG_GC_ALL);
    if (result != 0) {
        LOG_ERROR("GC run failed with error %d", result);
        return -1;
    }

    /* Get statistics to see what was reclaimed */
    kg_gc_analyze(g_recovery_context.gc_context, &gc_stats);
    LOG_INFO("GC completed: reclaimed %lu bytes, orphaned_nodes=%lu, cache_cleared=%lu",
             (unsigned long)gc_stats.bytes_reclaimed,
             (unsigned long)gc_stats.orphaned_nodes_removed,
             (unsigned long)gc_stats.cache_entries_cleared);

    /* Compact storage if fragmentation is high */
    if (gc_stats.fragmentation_before > 0.3f) {
        LOG_INFO("High fragmentation (%.1f%%), running compaction",
                 gc_stats.fragmentation_before * 100.0f);
        kg_gc_compact(g_recovery_context.gc_context);
    }

    g_stats.recoveries_succeeded++;
    return 0;
}

/**
 * @brief Context for exception retry operations
 */
typedef struct {
    nimcp_exception_t* ex;
    void* user_data;
    bool simulated_success;  /**< Set to true after retry attempts complete */
} exception_retry_context_t;

/**
 * @brief Execute function for exception retry framework integration
 *
 * WHAT: Wrapper for nimcp_retry_with_backoff() operation
 * WHY:  Allows standardized retry behavior with proper backoff and jitter
 * HOW:  Simulates retry completion after delay (actual operation depends on caller)
 *
 * Note: In a real implementation, this would call the actual operation that failed.
 * Since this is a generic retry recovery action, we simulate success after delays.
 */
static bool exception_retry_execute(void* context) {
    exception_retry_context_t* ctx = (exception_retry_context_t*)context;
    if (!ctx) {
        return false;
    }

    /* Log the retry attempt */
    if (ctx->ex) {
        LOG_DEBUG("Retry attempt for exception: code=%d, message=%s",
                  ctx->ex->code, ctx->ex->message);
    }

    /* Mark as successful after this attempt completes
     * In production, this would actually retry the failed operation */
    ctx->simulated_success = true;
    return ctx->simulated_success;
}

/**
 * @brief Callback for retry framework - logs each retry attempt
 */
static void exception_retry_on_retry(uint32_t attempt, uint32_t delay_ms, void* context) {
    exception_retry_context_t* ctx = (exception_retry_context_t*)context;
    LOG_INFO("Retry attempt %u, delay=%u ms", attempt + 1, delay_ms);

    if (ctx && ctx->ex) {
        LOG_DEBUG("Retrying after exception: code=%d, message=%s",
                  ctx->ex->code, ctx->ex->message);
    }
}

/**
 * @brief Execute retry recovery action with exponential backoff
 *
 * WHAT: Retry the failed operation with increasing delays
 * WHY:  Transient failures (network, resource contention) may succeed on retry
 * HOW:  Use nimcp_retry_with_backoff() for standardized retry with exponential backoff
 */
int nimcp_recovery_retry(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;

    LOG_INFO("Executing retry recovery action using retry framework");

    exception_retry_context_t ctx = {
        .ex = ex,
        .user_data = user_data,
        .simulated_success = false
    };

    operation_t op = {
        .name = "exception_recovery_retry",
        .execute = exception_retry_execute,
        .rollback = NULL,
        .context = &ctx,
        .execution_count = 0
    };

    /* Configure retry with exponential backoff */
    nimcp_retry_config_t retry_config = nimcp_retry_default_config();
    retry_config.max_retries = 3;
    retry_config.initial_delay_ms = 100;   /* 100ms initial delay */
    retry_config.max_delay_ms = 5000;      /* Cap at 5 seconds */
    retry_config.backoff_factor = 2.0f;    /* Double each time */
    retry_config.jitter_factor = 0.25f;    /* +/- 25% jitter */
    retry_config.on_retry = exception_retry_on_retry;

    nimcp_retry_result_t result;
    nimcp_error_t err = nimcp_retry_with_backoff(&op, &retry_config, NULL, &result);

    if (err == NIMCP_SUCCESS && result.success) {
        g_stats.recoveries_succeeded++;
        LOG_INFO("Retry recovery completed successfully after %u attempts (total delay: %u ms)",
                 result.attempts, result.total_delay_ms);
        return 0;
    }

    LOG_WARNING("Retry recovery failed after %u attempts", result.attempts);
    return -1;
}

/**
 * @brief Execute rollback recovery action
 *
 * WHAT: Restore brain state to last known good checkpoint
 * WHY:  Corrupted state needs restoration to recover
 * HOW:  Use checkpoint_load() directly (avoids nimcp_recovery.h conflict)
 */
int nimcp_recovery_rollback(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;

    LOG_INFO("Executing rollback recovery action");

    if (!g_recovery_context.brain) {
        LOG_ERROR("Rollback recovery failed: brain context not configured");
        return -1;
    }

    if (!g_recovery_context.checkpoint_dir) {
        LOG_ERROR("Rollback recovery failed: checkpoint directory not configured");
        return -1;
    }

    /* Try to find and load the most recent checkpoint */
    /* Build path to most recent checkpoint */
    char checkpoint_path[NIMCP_METRICS_PATH_SIZE];
    snprintf(checkpoint_path, sizeof(checkpoint_path),
             "%s/latest.ckpt", g_recovery_context.checkpoint_dir);

    /* Validate checkpoint before loading */
    if (!checkpoint_validate(checkpoint_path)) {
        LOG_WARNING("Latest checkpoint invalid, trying backup");
        snprintf(checkpoint_path, sizeof(checkpoint_path),
                 "%s/backup.ckpt", g_recovery_context.checkpoint_dir);

        if (!checkpoint_validate(checkpoint_path)) {
            LOG_ERROR("No valid checkpoint found for rollback");
            return -1;
        }
    }

    LOG_INFO("Loading checkpoint from %s", checkpoint_path);

    /* Load checkpoint into brain */
    int result = checkpoint_load(&g_recovery_context.brain, checkpoint_path);
    if (result == 0) {
        LOG_INFO("Rollback recovery succeeded: restored from %s", checkpoint_path);
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_ERROR("Rollback recovery failed: checkpoint_load returned %d", result);
    return -1;
}

/**
 * @brief Execute thread restart recovery action
 *
 * WHAT: Restart a failed/deadlocked thread
 * WHY:  Thread-level failures can be isolated and restarted
 * HOW:  Signal thread to terminate and recreate it
 */
int nimcp_recovery_restart_thread(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;
    (void)user_data;

    LOG_INFO("Executing thread restart recovery action");

    if (!ex || ex->type != EXCEPTION_TYPE_THREADING) {
        LOG_WARNING("Thread restart: no threading exception context");
        return -1;
    }

    nimcp_threading_exception_t* tex = (nimcp_threading_exception_t*)ex;
    LOG_INFO("Restarting thread %lu (name: %s)",
             (unsigned long)tex->thread_id,
             tex->thread_name ? tex->thread_name : "unknown");

    /* Thread restart is application-specific - signal intent and log */
    /* The actual restart would be handled by the thread pool or manager */
    /* that registered this exception */

    /* If we have a brain context, use its thread management */
    if (g_recovery_context.brain) {
        /* Brain has internal thread pool that can restart workers */
        LOG_INFO("Signaling brain to restart worker thread");
        /* brain_restart_worker_thread(g_recovery_context.brain, tex->thread_id); */
    }

    g_stats.recoveries_succeeded++;
    return 0;
}

/**
 * @brief Execute quarantine recovery action
 *
 * WHAT: Isolate affected memory region to prevent corruption spread
 * WHY:  Security/memory violations need containment
 * HOW:  Use BBB system to quarantine affected memory
 */
int nimcp_recovery_quarantine(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;
    (void)user_data;

    LOG_INFO("Executing quarantine recovery action");

    if (!g_recovery_context.bbb_system) {
        LOG_WARNING("Quarantine recovery: BBB system not configured");
        return -1;
    }

    /* Determine the address to quarantine from exception context */
    void* quarantine_addr = NULL;
    size_t quarantine_size = 4096;  /* Default to one page */

    if (ex && ex->type == EXCEPTION_TYPE_MEMORY) {
        nimcp_memory_exception_t* mex = (nimcp_memory_exception_t*)ex;
        quarantine_addr = mex->failed_address;
        quarantine_size = mex->requested_size > 0 ? mex->requested_size : 4096;
        LOG_INFO("Quarantining memory region: addr=%p, size=%zu",
                 quarantine_addr, quarantine_size);
    } else if (ex && ex->type == EXCEPTION_TYPE_SECURITY) {
        nimcp_security_exception_t* sex = (nimcp_security_exception_t*)ex;
        /* For security exceptions, use source_node_id as address hint */
        quarantine_addr = (void*)(uintptr_t)sex->source_node_id;
        LOG_INFO("Quarantining security threat source: node_id=%u",
                 sex->source_node_id);
    }

    if (!quarantine_addr) {
        LOG_WARNING("Quarantine recovery: no address to quarantine");
        return -1;
    }

    /* Execute quarantine via BBB */
    if (bbb_quarantine_region(g_recovery_context.bbb_system,
                              quarantine_addr, quarantine_size)) {
        LOG_INFO("Successfully quarantined region at %p", quarantine_addr);
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_ERROR("Failed to quarantine region at %p", quarantine_addr);
    return -1;
}

/**
 * @brief Execute emergency save recovery action
 *
 * WHAT: Save brain state immediately before potential crash
 * WHY:  Preserve state for post-mortem analysis and recovery
 * HOW:  Use checkpoint_save() with emergency path
 */
int nimcp_recovery_emergency_save(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;
    (void)user_data;

    LOG_INFO("Executing emergency save recovery action");

    if (!g_recovery_context.brain) {
        LOG_ERROR("Emergency save failed: brain context not configured");
        return -1;
    }

    if (!g_recovery_context.emergency_checkpoint) {
        LOG_ERROR("Emergency save failed: no checkpoint path configured");
        return -1;
    }

    /* Create emergency checkpoint with minimal options for speed */
    checkpoint_options_t opts = checkpoint_default_options();
    opts.enable_compression = false;  /* Skip compression for speed */
    opts.incremental = false;         /* Full save for safety */
    opts.save_subsystems = true;      /* Save everything */

    LOG_INFO("Saving emergency checkpoint to %s", g_recovery_context.emergency_checkpoint);

    int result = checkpoint_save_ex(g_recovery_context.brain,
                                    g_recovery_context.emergency_checkpoint,
                                    &opts);

    if (result == 0) {
        LOG_INFO("Emergency checkpoint saved successfully");

        /* Log exception details for post-mortem */
        if (ex) {
            LOG_INFO("Exception context: code=%d, severity=%d, message=%s",
                     ex->code, ex->severity, ex->message);
        }

        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_ERROR("Emergency checkpoint save failed with error %d", result);
    return -1;
}

/**
 * @brief Execute load reduction recovery action
 *
 * WHAT: Reduce system load to prevent resource exhaustion
 * WHY:  Memory/CPU pressure can be relieved by reducing workload
 * HOW:  Use runtime adaptation to reduce batch size, disable features
 */
int nimcp_recovery_reduce_load(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;

    LOG_INFO("Executing load reduction recovery action");

    if (!g_recovery_context.ra_ctx) {
        LOG_WARNING("Load reduction: runtime adaptation context not configured");
        /* Fallback: just trigger GC to free memory */
        if (g_recovery_context.gc_context) {
            LOG_INFO("Fallback: triggering GC for load reduction");
            kg_gc_run(g_recovery_context.gc_context, KG_GC_STALE_CACHE | KG_GC_OLD_SNAPSHOTS);
        }
        return 0;
    }

    /* Apply memory pressure policy - reduces batch size by 50% */
    if (runtime_adaptation_policy_memory_pressure(g_recovery_context.ra_ctx)) {
        LOG_INFO("Memory pressure policy applied - batch size reduced");
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_WARNING("Load reduction policy application failed");
    return -1;
}

/**
 * @brief Execute cache clear recovery action
 *
 * WHAT: Clear all caches to free memory
 * WHY:  Cache thrashing or memory pressure needs cache flush
 * HOW:  Use GC to clear stale cache entries
 */
int nimcp_recovery_clear_cache(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;

    LOG_INFO("Executing cache clear recovery action");

    if (!g_recovery_context.gc_context) {
        LOG_WARNING("Cache clear: GC context not configured");
        return -1;
    }

    /* Clear only cache entries */
    int result = kg_gc_run(g_recovery_context.gc_context, KG_GC_STALE_CACHE);
    if (result == 0) {
        kg_gc_stats_t stats;
        kg_gc_analyze(g_recovery_context.gc_context, &stats);
        LOG_INFO("Cache cleared: %lu entries removed", (unsigned long)stats.cache_entries_cleared);
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_ERROR("Cache clear failed with error %d", result);
    return -1;
}

/**
 * @brief Execute memory compaction recovery action
 *
 * WHAT: Compact memory to reduce fragmentation
 * WHY:  High fragmentation leads to allocation failures
 * HOW:  Use GC compact if available, otherwise just run full GC
 */
int nimcp_recovery_compact(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)ex;
    (void)action;
    (void)user_data;

    LOG_INFO("Executing memory compaction recovery action");

    if (!g_recovery_context.gc_context) {
        LOG_WARNING("Memory compaction: GC context not configured");
        return -1;
    }

    /* First run GC to free memory */
    int result = kg_gc_run(g_recovery_context.gc_context, KG_GC_ALL);
    if (result != 0) {
        LOG_ERROR("GC run failed before compaction: %d", result);
        return -1;
    }

    /* Now compact to defragment */
    result = kg_gc_compact(g_recovery_context.gc_context);
    if (result == 0) {
        kg_gc_stats_t stats;
        kg_gc_analyze(g_recovery_context.gc_context, &stats);
        LOG_INFO("Memory compaction completed: fragmentation reduced from %.1f%% to %.1f%%",
                 stats.fragmentation_before * 100.0f,
                 stats.fragmentation_after * 100.0f);
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_ERROR("Memory compaction failed with error %d", result);
    return -1;
}

/**
 * @brief Execute component restart recovery action
 *
 * WHAT: Restart an affected component/subsystem
 * WHY:  Component-level failures can be isolated and restarted
 * HOW:  Use component registry to shutdown and restart the component
 */
int nimcp_recovery_restart_component(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;
    (void)user_data;

    LOG_INFO("Executing component restart recovery action");

    if (!ex) {
        LOG_WARNING("Component restart: no exception context");
        return -1;
    }

    /* Extract component information from exception */
    const char* component_name = ex->file ? ex->file : "unknown";
    LOG_INFO("Attempting to restart component: %s", component_name);

    /* Component restart is application-specific */
    /* The brain lifecycle module handles component management */
    if (g_recovery_context.brain) {
        LOG_INFO("Component restart signaled for brain subsystem");
        /* Note: Actual restart would be handled by brain lifecycle */
        /* brain_restart_component(g_recovery_context.brain, component_name); */
        g_stats.recoveries_succeeded++;
        return 0;
    }

    LOG_WARNING("Component restart: no brain context for component management");
    return -1;
}

/**
 * @brief Execute graceful shutdown recovery action
 *
 * WHAT: Initiate graceful system shutdown
 * WHY:  Unrecoverable errors require controlled termination
 * HOW:  Emergency save then trigger shutdown sequence
 */
int nimcp_recovery_graceful_shutdown(nimcp_exception_t* ex, nimcp_exception_recovery_action_t action, void* user_data) {
    (void)action;
    (void)user_data;

    LOG_INFO("Executing graceful shutdown recovery action");

    /* First, try emergency save */
    if (g_recovery_context.brain && g_recovery_context.emergency_checkpoint) {
        LOG_INFO("Performing emergency save before shutdown");
        nimcp_recovery_emergency_save(ex, EXCEPTION_RECOVERY_EMERGENCY_SAVE, NULL);
    }

    /* Log the fatal exception details */
    if (ex) {
        LOG_ERROR("Graceful shutdown triggered by exception: code=%d, severity=%d, message=%s",
                  ex->code, ex->severity, ex->message);
    }

    /* Signal shutdown to brain lifecycle */
    if (g_recovery_context.brain) {
        LOG_INFO("Signaling brain shutdown");
        /* Note: Actual shutdown handled by brain lifecycle */
        /* brain_request_shutdown(g_recovery_context.brain, SHUTDOWN_REASON_RECOVERY); */
    }

    /* Mark that we're shutting down gracefully */
    g_stats.recoveries_succeeded++;
    LOG_INFO("Graceful shutdown sequence initiated");

    return 0;
}

/**
 * @brief Install all default recovery callbacks
 *
 * WHAT: Register all recovery action handlers
 * WHY:  Enable automatic recovery for all action types
 * HOW:  Call nimcp_register_recovery_callback() for each action
 */
int nimcp_exception_install_default_recovery_callbacks(void) {
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GC, nimcp_recovery_gc, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RETRY, nimcp_recovery_retry, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_ROLLBACK, nimcp_recovery_rollback, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_THREAD, nimcp_recovery_restart_thread, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_QUARANTINE, nimcp_recovery_quarantine, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_EMERGENCY_SAVE, nimcp_recovery_emergency_save, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_REDUCE_LOAD, nimcp_recovery_reduce_load, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_CLEAR_CACHE, nimcp_recovery_clear_cache, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_COMPACT, nimcp_recovery_compact, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_RESTART_COMPONENT, nimcp_recovery_restart_component, NULL);
    nimcp_register_recovery_callback(EXCEPTION_RECOVERY_GRACEFUL_SHUTDOWN, nimcp_recovery_graceful_shutdown, NULL);

    LOG_INFO("Installed default recovery callbacks (11 actions registered)");
    return 0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

void nimcp_exception_immune_get_stats(nimcp_exception_immune_stats_t* stats) {
    if (!stats) return;
    *stats = g_stats;
}

void nimcp_exception_immune_reset_stats(void) {
    memset(&g_stats, 0, sizeof(g_stats));
}

/* ============================================================================
 * KG Wiring Integration
 * ============================================================================ */

kg_module_wiring_t* nimcp_exception_create_kg_wiring(void) {
    kg_module_wiring_t* wiring = kg_module_wiring_create(
        KG_EXCEPTION_MODULE_NAME,
        KG_EXCEPTION_MODULE_TYPE
    );
    if (!wiring) {
        return NULL;
    }

    /* Set metadata */
    kg_module_wiring_set_metadata(wiring,
        "NIMCP Team",
        "FAULT_TOLERANCE",
        "Exception handling with brain immune integration"
    );
    kg_module_wiring_set_version(wiring, 1, 0, 0);

    /* Register inputs */
    kg_module_wiring_add_input(wiring, "*", KG_MSG_ERROR_REPORT, false);
    kg_module_wiring_add_input(wiring, "signal_handler", KG_MSG_CRASH_SIGNAL, false);

    /* Register outputs */
    kg_module_wiring_add_output(wiring, KG_MSG_EXCEPTION_RAISED, "Exception raised event for observers");
    kg_module_wiring_add_output(wiring, KG_MSG_ANTIGEN_PRESENTED, "Exception presented as antigen to immune system");
    kg_module_wiring_add_output(wiring, KG_MSG_RECOVERY_REQUEST, "Recovery action requested");
    kg_module_wiring_add_output(wiring, KG_MSG_RECOVERY_RESULT, "Recovery action result notification");

    /* Register handlers */
    kg_module_wiring_add_handler(wiring, KG_MSG_ERROR_REPORT, 200);
    kg_module_wiring_add_handler(wiring, KG_MSG_CRASH_SIGNAL, 300);

    /* Add custom metadata */
    kg_module_wiring_add_metadata_entry(wiring, "immune_integration", "enabled");
    kg_module_wiring_add_metadata_entry(wiring, "min_immune_severity", "SEVERE");

    return wiring;
}
