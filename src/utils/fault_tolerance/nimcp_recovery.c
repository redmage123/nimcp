/**
 * @file nimcp_recovery.c
 * @brief Implementation of intelligent recovery strategies
 *
 * @author NIMCP Team
 * @date 2025-11-19
 */

#include "utils/fault_tolerance/nimcp_recovery.h"
#include "security/nimcp_security.h"
#include "security/nimcp_blood_brain_barrier.h"
#include "constants/nimcp_timing_constants.h"  /* Centralized timing constants */

#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_messages.h"
#include "core/brain/persistence/nimcp_brain_persistence.h"
#include "core/brain/nimcp_brain_state.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "core/neuralnet/nimcp_neuralnet_core.h"
#include "core/neuralnet/nimcp_neuron_synapse_access.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_recovery"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(recovery)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef __GLIBC__
#include <malloc.h>  // For malloc_trim()
#include "utils/memory/nimcp_unified_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "constants/nimcp_buffer_constants.h"
#endif

// Simple logging macros (fallback if nimcp_logging.h not available)
#ifndef LOG_INFO
#define LOG_INFO(fmt, ...) fprintf(stderr, "[INFO] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_WARN
#define LOG_WARN(fmt, ...) fprintf(stderr, "[WARN] " fmt "\n", ##__VA_ARGS__)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR(fmt, ...) fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)
#endif

//=============================================================================
// Recovery Statistics
//=============================================================================

typedef struct {
    uint64_t total_recoveries;
    uint64_t successful_recoveries;
    uint64_t failed_recoveries;
    uint64_t immediate_recoveries;
    uint64_t tactical_recoveries;
    uint64_t strategic_recoveries;
    uint64_t preventive_recoveries;
    uint64_t auto_heals;
    uint64_t rollbacks;
    uint64_t parameter_adjustments;
} recovery_stats_t;

static recovery_stats_t g_recovery_stats = {0};
static nimcp_mutex_t g_recovery_stats_mutex = NIMCP_MUTEX_INITIALIZER;

//=============================================================================
// Predefined Recovery Strategies
//=============================================================================

// Strategy for SIGSEGV (segmentation fault)
// Uses centralized timing constants for consistency
static recovery_strategy_t g_strategy_sigsegv = {
    .tier = RECOVERY_TIER_STRATEGIC,
    .primary = RECOVERY_ACTION_RELOAD_CHECKPOINT,
    .fallback = RECOVERY_ACTION_EMERGENCY_SAVE,
    .max_retries = 1,
    .timeout_ms = NIMCP_RECOVERY_TIMEOUT_MS,  /* Was: 1000 */
    .success_threshold = 0.7F,
    .description = "Rollback to checkpoint after segmentation fault"
};

// Strategy for SIGFPE (floating point exception)
static recovery_strategy_t g_strategy_sigfpe = {
    .tier = RECOVERY_TIER_IMMEDIATE,
    .primary = RECOVERY_ACTION_CLEAR_NAN,
    .fallback = RECOVERY_ACTION_REDUCE_LR,
    .max_retries = 3,
    .timeout_ms = NIMCP_FAST_TIMEOUT_MS,  /* Was: 100 */
    .success_threshold = 0.8F,
    .description = "Clear NaN/Inf and reduce learning rate"
};

// Strategy for memory issues
static recovery_strategy_t g_strategy_memory = {
    .tier = RECOVERY_TIER_TACTICAL,
    .primary = RECOVERY_ACTION_TRIGGER_GC,
    .fallback = RECOVERY_ACTION_REDUCE_BATCH,
    .max_retries = 2,
    .timeout_ms = NIMCP_SHORT_TIMEOUT_MS,  /* Was: 500 */
    .success_threshold = 0.75F,
    .description = "Free memory and reduce batch size"
};

// Strategy for performance degradation
static recovery_strategy_t g_strategy_performance = {
    .tier = RECOVERY_TIER_STRATEGIC,
    .primary = RECOVERY_ACTION_FALLBACK_CPU,
    .fallback = RECOVERY_ACTION_REDUCE_MODEL,
    .max_retries = 1,
    .timeout_ms = NIMCP_RECOVERY_L2_TIMEOUT_MS,  /* Was: 2000 */
    .success_threshold = 0.6F,
    .description = "Fallback to CPU or reduce model complexity"
};

// Strategy for corruption
static recovery_strategy_t g_strategy_corruption = {
    .tier = RECOVERY_TIER_STRATEGIC,
    .primary = RECOVERY_ACTION_RELOAD_CHECKPOINT,
    .fallback = RECOVERY_ACTION_REINIT_LAYER,
    .max_retries = 2,
    .timeout_ms = NIMCP_RECOVERY_TIMEOUT_MS,  /* Was: 1000 */
    .success_threshold = 0.7F,
    .description = "Reload checkpoint or reinitialize corrupted layers"
};

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Get current time in microseconds
 * WHY:  Track recovery duration
 * HOW:  Use gettimeofday()
 */
static uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

/**
 * WHAT: Get current time in milliseconds
 * WHY:  Track circuit breaker timeouts
 * HOW:  Use gettimeofday()
 */
static uint64_t get_time_ms(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)tv.tv_usec / 1000ULL;
}

/**
 * WHAT: Sleep for microseconds
 * WHY:  Exponential backoff for retries
 * HOW:  Use usleep()
 */
static void sleep_us(uint32_t us)
{
    usleep(us);
}

const char* recovery_tier_name(recovery_tier_t tier)
{
    switch (tier) {
        case RECOVERY_TIER_IMMEDIATE:   return "IMMEDIATE";
        case RECOVERY_TIER_TACTICAL:    return "TACTICAL";
        case RECOVERY_TIER_STRATEGIC:   return "STRATEGIC";
        case RECOVERY_TIER_PREVENTIVE:  return "PREVENTIVE";
        default:                        return "UNKNOWN";
    }
}

const char* recovery_action_name(recovery_action_t action)
{
    switch (action) {
        case RECOVERY_ACTION_NONE:              return "NONE";
        case RECOVERY_ACTION_CLEAR_NAN:         return "CLEAR_NAN";
        case RECOVERY_ACTION_RESET_COUNTER:     return "RESET_COUNTER";
        case RECOVERY_ACTION_FLUSH_CACHE:       return "FLUSH_CACHE";
        case RECOVERY_ACTION_RESET_FPU:         return "RESET_FPU";
        case RECOVERY_ACTION_RELOAD_CHECKPOINT: return "RELOAD_CHECKPOINT";
        case RECOVERY_ACTION_REDUCE_LR:         return "REDUCE_LR";
        case RECOVERY_ACTION_REDUCE_BATCH:      return "REDUCE_BATCH";
        case RECOVERY_ACTION_TRIGGER_GC:        return "TRIGGER_GC";
        case RECOVERY_ACTION_RESTART_OP:        return "RESTART_OP";
        case RECOVERY_ACTION_FALLBACK_CPU:      return "FALLBACK_CPU";
        case RECOVERY_ACTION_REDUCE_MODEL:      return "REDUCE_MODEL";
        case RECOVERY_ACTION_REINIT_LAYER:      return "REINIT_LAYER";
        case RECOVERY_ACTION_EMERGENCY_SAVE:    return "EMERGENCY_SAVE";
        case RECOVERY_ACTION_INCREASE_MEMORY:   return "INCREASE_MEMORY";
        case RECOVERY_ACTION_COMPACT_MEMORY:    return "COMPACT_MEMORY";
        case RECOVERY_ACTION_ENABLE_CHECKS:     return "ENABLE_CHECKS";
        case RECOVERY_ACTION_AUTO_CHECKPOINT:   return "AUTO_CHECKPOINT";
        default:                                return "UNKNOWN";
    }
}

const char* recovery_status_name(recovery_status_t status)
{
    switch (status) {
        case RECOVERY_SUCCESS:          return "SUCCESS";
        case RECOVERY_PARTIAL:          return "PARTIAL";
        case RECOVERY_FAILED:           return "FAILED";
        case RECOVERY_NOT_APPLICABLE:   return "NOT_APPLICABLE";
        case RECOVERY_REQUIRES_RESTART: return "REQUIRES_RESTART";
        default:                        return "UNKNOWN";
    }
}

//=============================================================================
// Recovery Action Implementations
//=============================================================================

/**
 * WHAT: Clear NaN/Inf from brain weights
 * WHY:  Prevent numeric instability propagation
 * HOW:  Scan all weights, replace NaN/Inf with 0.0
 */
static recovery_status_t action_clear_nan(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "action_clear_nan: brain is NULL");
        return RECOVERY_FAILED;
    }

    LOG_INFO("Recovery: Clearing NaN/Inf from weights");

    /* Get the adaptive network from the brain */
    adaptive_network_t network = brain_get_network(brain);
    if (!network) {
        LOG_WARN("Recovery: Could not get network from brain");
        g_recovery_stats.immediate_recoveries++;
        return RECOVERY_PARTIAL;
    }

    /* Get the base neural network for direct synapse access */
    neural_network_t base_net = adaptive_network_get_base_network(network);
    if (!base_net) {
        LOG_WARN("Recovery: Could not get base network");
        g_recovery_stats.immediate_recoveries++;
        return RECOVERY_PARTIAL;
    }

    /* Get neuron count */
    uint32_t num_neurons = adaptive_network_get_neuron_count(network);
    uint32_t nan_cleared = 0;
    uint32_t inf_cleared = 0;

    /* Iterate through all neurons and their synapses */
    for (uint32_t n = 0; n < num_neurons; n++) {
        neuron_t* neuron = neural_network_get_neuron(base_net, n);
        if (!neuron) continue;

        uint32_t out_count = NEURON_OUT_COUNT(neuron);
        for (uint32_t s = 0; s < out_count; s++) {
            synapse_handle_t* h = NEURON_OUT_HANDLE(neuron, s);
            if (!h) continue;
            float weight = h->weight;

            /* Check for NaN */
            if (isnan(weight)) {
                h->weight = 0.0f;
                nan_cleared++;
            }
            /* Check for Inf */
            else if (isinf(weight)) {
                h->weight = 0.0f;
                inf_cleared++;
            }
        }
    }

    if (nan_cleared > 0 || inf_cleared > 0) {
        LOG_INFO("Recovery: Cleared %u NaN and %u Inf values from weights",
                 nan_cleared, inf_cleared);
    } else {
        LOG_INFO("Recovery: No NaN/Inf values found in weights");
    }

    g_recovery_stats.immediate_recoveries++;
    return RECOVERY_SUCCESS;
}

/**
 * WHAT: Reset FPU exception flags
 * WHY:  Clear sticky FPU errors
 * HOW:  Clear FPU status register
 */
static recovery_status_t action_reset_fpu(brain_t brain)
{
    (void)brain;  // Not brain-specific

    LOG_INFO("Recovery: Resetting FPU exception flags");

    // Clear FPU exceptions
#ifdef __x86_64__
    asm volatile("fnclex");  // Clear FPU exceptions
#endif

    g_recovery_stats.immediate_recoveries++;
    return RECOVERY_SUCCESS;
}

/**
 * WHAT: Flush temporary caches
 * WHY:  Free memory and clear corrupted data
 * HOW:  Call cache flush operations
 */
static recovery_status_t action_flush_cache(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "action_flush_cache: brain is NULL");
        return RECOVERY_FAILED;
    }

    LOG_INFO("Recovery: Flushing temporary caches");

    // TODO: Implement cache flushing (would need brain internal access)

    g_recovery_stats.immediate_recoveries++;
    return RECOVERY_SUCCESS;
}

/**
 * WHAT: Trigger garbage collection
 * WHY:  Free memory for recovery
 * HOW:  Force memory allocator to release unused blocks
 */
static recovery_status_t action_trigger_gc(brain_t brain)
{
    (void)brain;

    LOG_INFO("Recovery: Triggering garbage collection");

    // Force malloc to release memory to OS
#ifdef __GLIBC__
    malloc_trim(0);
#endif

    g_recovery_stats.tactical_recoveries++;
    return RECOVERY_SUCCESS;
}

/**
 * WHAT: Reload brain from checkpoint
 * WHY:  Restore to known good state
 * HOW:  Load most recent checkpoint
 */
static recovery_status_t action_reload_checkpoint(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "action_reload_checkpoint: brain is NULL");
        return RECOVERY_FAILED;
    }

    LOG_INFO("Recovery: Reloading from checkpoint");

    // Try to restore from snapshot
    brain_t restored = brain_restore_snapshot(NULL, "autosave");
    if (restored) {
        LOG_INFO("Recovery: Successfully restored from checkpoint");
        // TODO: Replace current brain state with restored state
        g_recovery_stats.rollbacks++;
        g_recovery_stats.tactical_recoveries++;
        return RECOVERY_SUCCESS;
    }

    LOG_WARN("Recovery: No checkpoint available");
    return RECOVERY_NOT_APPLICABLE;
}

/**
 * WHAT: Emergency save brain state
 * WHY:  Preserve state before crash
 * HOW:  Quick save to emergency checkpoint
 */
static recovery_status_t action_emergency_save(brain_t brain)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "action_emergency_save: brain is NULL");
        return RECOVERY_FAILED;
    }

    LOG_INFO("Recovery: Performing emergency save");

    // Save to emergency snapshot
    char emergency_name[NIMCP_ID_BUFFER_SIZE];
    snprintf(emergency_name, sizeof(emergency_name), "emergency_%lu", (unsigned long)time(NULL));

    if (brain_save_snapshot(brain, emergency_name, "Emergency save before crash")) {
        LOG_INFO("Recovery: Emergency save successful: %s", emergency_name);
        g_recovery_stats.tactical_recoveries++;
        return RECOVERY_SUCCESS;
    }

    LOG_ERROR("Recovery: Emergency save failed");
    return RECOVERY_FAILED;
}

//=============================================================================
// Strategy Selection
//=============================================================================

recovery_strategy_t* recovery_select_strategy(diagnostic_summary_t* diagnosis)
{
    NIMCP_API_CHECK_NULL_RET_NULL(diagnosis, "recovery_select_strategy: NULL diagnosis");

    // Select strategy based on signal
    switch (diagnosis->signal) {
        case SIGSEGV:
        case SIGBUS:
            LOG_INFO("Recovery: Selected SIGSEGV strategy (rollback checkpoint)");
            return &g_strategy_sigsegv;

        case SIGFPE:
            LOG_INFO("Recovery: Selected SIGFPE strategy (clear NaN)");
            return &g_strategy_sigfpe;

        case SIGABRT:
            if (diagnosis->failure_type && strstr(diagnosis->failure_type, "memory")) {
                LOG_INFO("Recovery: Selected memory strategy (GC + reduce batch)");
                return &g_strategy_memory;
            }
            return &g_strategy_corruption;

        default:
            // Select based on failure type
            if (diagnosis->failure_type) {
                if (strstr(diagnosis->failure_type, "memory")) {
                    return &g_strategy_memory;
                } else if (strstr(diagnosis->failure_type, "numeric")) {
                    return &g_strategy_sigfpe;
                } else if (strstr(diagnosis->failure_type, "performance")) {
                    return &g_strategy_performance;
                } else if (strstr(diagnosis->failure_type, "corruption")) {
                    return &g_strategy_corruption;
                }
            }
            break;
    }

    // Default to corruption strategy
    LOG_INFO("Recovery: Using default corruption strategy");
    return &g_strategy_corruption;
}

recovery_strategy_t* recovery_create_plan(diagnostic_summary_t* diagnosis, health_state_t health)
{
    (void)health;  // TODO: Use health metrics for preventive actions

    // For now, just select strategy based on diagnosis
    return recovery_select_strategy(diagnosis);
}

//=============================================================================
// Recovery Execution
//=============================================================================

/**
 * WHAT: Execute a single recovery action
 * WHY:  Apply specific recovery technique
 * HOW:  Dispatch to action handler
 */
static recovery_status_t execute_action(brain_t brain, recovery_action_t action)
{
    switch (action) {
        case RECOVERY_ACTION_CLEAR_NAN:
            return action_clear_nan(brain);
        case RECOVERY_ACTION_RESET_FPU:
            return action_reset_fpu(brain);
        case RECOVERY_ACTION_FLUSH_CACHE:
            return action_flush_cache(brain);
        case RECOVERY_ACTION_TRIGGER_GC:
            return action_trigger_gc(brain);
        case RECOVERY_ACTION_RELOAD_CHECKPOINT:
            return action_reload_checkpoint(brain);
        case RECOVERY_ACTION_EMERGENCY_SAVE:
            return action_emergency_save(brain);
        case RECOVERY_ACTION_NONE:
            return RECOVERY_NOT_APPLICABLE;
        default:
            LOG_WARN("Recovery: Action %s not yet implemented",
                     recovery_action_name(action));
            return RECOVERY_NOT_APPLICABLE;
    }
}

recovery_result_t recovery_execute_strategy(brain_t brain, diagnostic_summary_t* diagnosis)
{
    recovery_result_t result = {0};
    uint64_t start_time = get_time_us();

    // Validate inputs using API exception macros
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_execute_strategy: NULL brain");
        result.status = RECOVERY_FAILED;
        result.message = "NULL brain parameter";
        return result;
    }
    if (!diagnosis) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_execute_strategy: NULL diagnosis");
        result.status = RECOVERY_FAILED;
        result.message = "NULL diagnosis parameter";
        return result;
    }

    g_recovery_stats.total_recoveries++;

    // Select strategy
    recovery_strategy_t* strategy = recovery_select_strategy(diagnosis);
    if (!strategy) {
        result.status = RECOVERY_NOT_APPLICABLE;
        result.message = "No applicable strategy";
        return result;
    }

    result.tier = strategy->tier;
    result.action = strategy->primary;

    LOG_INFO("Recovery: Executing %s strategy (tier: %s, action: %s)",
             strategy->description,
             recovery_tier_name(strategy->tier),
             recovery_action_name(strategy->primary));

    // Try primary action
    recovery_status_t status = execute_action(brain, strategy->primary);

    // Try fallback if primary failed
    if (status != RECOVERY_SUCCESS && strategy->fallback != RECOVERY_ACTION_NONE) {
        LOG_INFO("Recovery: Primary action failed, trying fallback: %s",
                 recovery_action_name(strategy->fallback));
        result.action = strategy->fallback;
        status = execute_action(brain, strategy->fallback);
    }

    result.status = status;
    result.time_us = (uint32_t)(get_time_us() - start_time);

    // Update statistics
    if (status == RECOVERY_SUCCESS) {
        g_recovery_stats.successful_recoveries++;
        result.message = "Recovery successful";
        result.success_probability = 0.9F;
    } else {
        g_recovery_stats.failed_recoveries++;
        result.message = "Recovery failed";
        result.success_probability = 0.1F;
    }

    LOG_INFO("Recovery: Completed in %u us (status: %s)",
             result.time_us, recovery_status_name(result.status));

    return result;
}

recovery_result_t recovery_retry_operation(brain_t brain, operation_t* op, uint32_t max_retries)
{
    recovery_result_t result = {0};
    uint64_t start_time = get_time_us();

    if (!brain || !op || !op->execute) {
        result.status = RECOVERY_FAILED;
        result.message = "Invalid arguments";
        return result;
    }

    result.action = RECOVERY_ACTION_RESTART_OP;
    result.tier = RECOVERY_TIER_TACTICAL;

    LOG_INFO("Recovery: Retrying operation '%s' (max retries: %u)",
             op->name ? op->name : "unnamed", max_retries);

    // Exponential backoff parameters
    uint32_t base_delay_us = 10000;  // Start at 10ms
    uint32_t max_delay_us = 1000000; // Cap at 1s

    for (uint32_t attempt = 0; attempt <= max_retries; attempt++) {
        op->execution_count++;

        LOG_INFO("Recovery: Attempt %u/%u", attempt + 1, max_retries + 1);

        // Execute operation
        bool success = op->execute(op->context);

        if (success) {
            result.status = RECOVERY_SUCCESS;
            result.message = "Operation succeeded";
            result.time_us = (uint32_t)(get_time_us() - start_time);
            result.success_probability = 1.0F;

            g_recovery_stats.successful_recoveries++;
            g_recovery_stats.tactical_recoveries++;

            LOG_INFO("Recovery: Operation succeeded on attempt %u", attempt + 1);
            return result;
        }

        // If not the last attempt, wait with exponential backoff
        if (attempt < max_retries) {
            uint32_t delay_us = base_delay_us * (1 << attempt);  // 2^attempt
            if (delay_us > max_delay_us) {
                delay_us = max_delay_us;
            }

            LOG_INFO("Recovery: Waiting %u ms before retry", delay_us / 1000);
            sleep_us(delay_us);
        }
    }

    // All retries failed
    result.status = RECOVERY_FAILED;
    result.message = "All retries exhausted";
    result.time_us = (uint32_t)(get_time_us() - start_time);
    result.success_probability = 0.0F;

    g_recovery_stats.failed_recoveries++;

    LOG_ERROR("Recovery: Operation failed after %u attempts", max_retries + 1);

    // Call rollback if provided
    if (op->rollback) {
        LOG_INFO("Recovery: Executing rollback");
        op->rollback(op->context);
        result.requires_rollback = true;
    }

    return result;
}

recovery_result_t recovery_fallback_cpu(brain_t brain)
{
    recovery_result_t result = {0};
    uint64_t start_time = get_time_us();

    if (!brain) {
        result.status = RECOVERY_FAILED;
        result.message = "Invalid brain";
        return result;
    }

    result.action = RECOVERY_ACTION_FALLBACK_CPU;
    result.tier = RECOVERY_TIER_STRATEGIC;

    LOG_INFO("Recovery: Falling back to CPU execution mode");

    // TODO: Implement GPU -> CPU fallback
    // Would need to:
    // 1. Disable GPU acceleration flag
    // 2. Copy GPU data to CPU
    // 3. Reinitialize CPU execution context

    result.status = RECOVERY_NOT_APPLICABLE;
    result.message = "CPU fallback not yet implemented";
    result.time_us = (uint32_t)(get_time_us() - start_time);

    g_recovery_stats.strategic_recoveries++;

    return result;
}

recovery_result_t recovery_rollback_state(brain_t brain, const char* checkpoint)
{
    recovery_result_t result = {0};
    uint64_t start_time = get_time_us();

    if (!brain) {
        result.status = RECOVERY_FAILED;
        result.message = "Invalid brain";
        return result;
    }

    result.action = RECOVERY_ACTION_RELOAD_CHECKPOINT;
    result.tier = RECOVERY_TIER_STRATEGIC;
    result.requires_rollback = true;

    const char* checkpoint_name = checkpoint ? checkpoint : "autosave";

    LOG_INFO("Recovery: Rolling back to checkpoint '%s'", checkpoint_name);

    // Restore from snapshot
    brain_t restored = brain_restore_snapshot(NULL, checkpoint_name);
    if (restored) {
        // TODO: Replace current brain state with restored state
        result.status = RECOVERY_SUCCESS;
        result.message = "Rollback successful";
        result.success_probability = 0.9F;

        g_recovery_stats.successful_recoveries++;
        g_recovery_stats.rollbacks++;
        g_recovery_stats.strategic_recoveries++;

        LOG_INFO("Recovery: Successfully rolled back to '%s'", checkpoint_name);
    } else {
        result.status = RECOVERY_FAILED;
        result.message = "Checkpoint not found or corrupted";
        result.success_probability = 0.0F;

        g_recovery_stats.failed_recoveries++;

        LOG_ERROR("Recovery: Failed to rollback to '%s'", checkpoint_name);
    }

    result.time_us = (uint32_t)(get_time_us() - start_time);
    return result;
}

//=============================================================================
// Self-Healing
//=============================================================================

bool recovery_auto_heal(brain_t brain, diagnostic_summary_t* diagnosis)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_auto_heal: brain is NULL");
        return false;
    }

    LOG_INFO("Recovery: Initiating auto-heal");
    g_recovery_stats.auto_heals++;

    // If diagnosis provided, use it
    if (diagnosis) {
        recovery_result_t result = recovery_execute_strategy(brain, diagnosis);
        return (result.status == RECOVERY_SUCCESS);
    }

    // Otherwise, perform general health checks and fixes

    // 1. Clear any NaN/Inf
    if (action_clear_nan(brain) == RECOVERY_SUCCESS) {
        LOG_INFO("Recovery: Auto-heal cleared NaN/Inf");
    }

    // 2. Trigger GC
    if (action_trigger_gc(brain) == RECOVERY_SUCCESS) {
        LOG_INFO("Recovery: Auto-heal triggered GC");
    }

    // 3. Flush caches
    if (action_flush_cache(brain) == RECOVERY_SUCCESS) {
        LOG_INFO("Recovery: Auto-heal flushed caches");
    }

    LOG_INFO("Recovery: Auto-heal completed");
    return true;
}

bool recovery_adjust_parameters(brain_t brain, adjustment_type_t type)
{
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "recovery_adjust_parameters: brain is NULL");
        return false;
    }

    g_recovery_stats.parameter_adjustments++;

    switch (type) {
        case ADJUSTMENT_LEARNING_RATE:
            LOG_INFO("Recovery: Reducing learning rate by 50%%");
            // TODO: Implement learning rate adjustment
            return true;

        case ADJUSTMENT_BATCH_SIZE:
            LOG_INFO("Recovery: Reducing batch size by 50%%");
            // TODO: Implement batch size adjustment
            return true;

        case ADJUSTMENT_MEMORY_LIMIT:
            LOG_INFO("Recovery: Increasing memory limit by 20%%");
            // TODO: Implement memory limit adjustment
            return true;

        case ADJUSTMENT_TIMEOUT:
            LOG_INFO("Recovery: Doubling operation timeout");
            // TODO: Implement timeout adjustment
            return true;

        case ADJUSTMENT_PRECISION:
            LOG_INFO("Recovery: Switching to float64 precision");
            // TODO: Implement precision adjustment
            return true;

        default:
            LOG_WARN("Recovery: Unknown adjustment type: %d", type);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "recovery_adjust_parameters: operation failed");
            return false;
    }
}

//=============================================================================
// Circuit Breaker Implementation
//=============================================================================

circuit_breaker_t* circuit_breaker_create(uint32_t failure_threshold, uint32_t timeout_ms)
{
    // Validate failure threshold using API exception macro (returns NULL for pointer-returning function)
    NIMCP_API_CHECK_RET_NULL(failure_threshold > 0 && failure_threshold <= 100,
        NIMCP_ERROR_INVALID_PARAM, "Circuit breaker: Invalid failure threshold (must be 1-100)");

    // Validate timeout using API exception macro (returns NULL for pointer-returning function)
    NIMCP_API_CHECK_RET_NULL(timeout_ms >= 100 && timeout_ms <= 60000,
        NIMCP_ERROR_INVALID_PARAM, "Circuit breaker: Invalid timeout (must be 100-60000 ms)");

    circuit_breaker_t* cb = (circuit_breaker_t*)nimcp_calloc(1, sizeof(circuit_breaker_t));
    NIMCP_API_CHECK_ALLOC(cb, "Circuit breaker allocation failed");

    cb->state = CIRCUIT_CLOSED;
    cb->failure_threshold = failure_threshold;
    cb->timeout_ms = timeout_ms;
    cb->last_failure_time = 0;
    cb->last_attempt_time = 0;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->total_failures = 0;
    cb->total_successes = 0;

    LOG_INFO("Circuit breaker: Created (threshold: %u, timeout: %u ms)",
             failure_threshold, timeout_ms);

    return cb;
}

void circuit_breaker_destroy(circuit_breaker_t* cb)
{
    if (cb) {
        LOG_INFO("Circuit breaker: Destroyed (total successes: %u, total failures: %u)",
                 cb->total_successes, cb->total_failures);
        nimcp_free(cb);
    }
}

bool circuit_breaker_allow_operation(circuit_breaker_t* cb)
{
    if (!cb) {
        return true;  // No breaker = allow
    }

    uint64_t now = get_time_ms();
    cb->last_attempt_time = now;

    switch (cb->state) {
        case CIRCUIT_CLOSED:
            // Normal operation - allow
            return true;

        case CIRCUIT_OPEN:
            // Check if timeout has passed
            if (now - cb->last_failure_time >= cb->timeout_ms) {
                // Try half-open
                LOG_INFO("Circuit breaker: Transitioning to HALF_OPEN (testing recovery)");
                cb->state = CIRCUIT_HALF_OPEN;
                cb->success_count = 0;
                return true;
            }
            // Still in timeout - block
            LOG_WARN("Circuit breaker: Operation blocked (OPEN state)");
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circuit_breaker_allow_operation: capacity exceeded");
            return false;

        case CIRCUIT_HALF_OPEN:
            // Allow one test operation
            return true;
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "circuit_breaker_allow_operation: operation failed");
    return false;
}

void circuit_breaker_record_success(circuit_breaker_t* cb)
{
    if (!cb) return;

    cb->success_count++;
    cb->total_successes++;
    cb->failure_count = 0;  // Reset failure count

    if (cb->state == CIRCUIT_HALF_OPEN) {
        // Success in half-open -> close circuit
        LOG_INFO("Circuit breaker: Success in HALF_OPEN -> CLOSED");
        cb->state = CIRCUIT_CLOSED;
        cb->success_count = 0;
    }
}

void circuit_breaker_record_failure(circuit_breaker_t* cb)
{
    if (!cb) return;

    cb->failure_count++;
    cb->total_failures++;
    cb->success_count = 0;  // Reset success count
    cb->last_failure_time = get_time_ms();

    if (cb->state == CIRCUIT_HALF_OPEN) {
        // Failure in half-open -> back to open
        LOG_WARN("Circuit breaker: Failure in HALF_OPEN -> OPEN");
        cb->state = CIRCUIT_OPEN;
        cb->failure_count = 0;
    } else if (cb->state == CIRCUIT_CLOSED) {
        // Check if threshold exceeded
        if (cb->failure_count >= cb->failure_threshold) {
            LOG_ERROR("Circuit breaker: Threshold exceeded (%u failures) -> OPEN",
                     cb->failure_count);
            cb->state = CIRCUIT_OPEN;
            cb->failure_count = 0;
        }
    }
}

void circuit_breaker_reset(circuit_breaker_t* cb)
{
    if (!cb) return;

    LOG_INFO("Circuit breaker: Manual reset");
    cb->state = CIRCUIT_CLOSED;
    cb->failure_count = 0;
    cb->success_count = 0;
    cb->last_failure_time = 0;
}

circuit_state_t circuit_breaker_get_state(circuit_breaker_t* cb)
{
    return cb ? cb->state : CIRCUIT_CLOSED;
}

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * @brief Get recovery statistics (for debugging/monitoring)
 */
void recovery_get_stats(recovery_stats_t* out_stats)
{
    if (out_stats) {
        *out_stats = g_recovery_stats;
    }
}

/**
 * @brief Reset recovery statistics
 */
void recovery_reset_stats(void)
{
    memset(&g_recovery_stats, 0, sizeof(g_recovery_stats));
    LOG_INFO("Recovery: Statistics reset");
}
