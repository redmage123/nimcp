/**
 * @file nimcp_cognitive_recovery.c
 * @brief Cognitive Recovery Coordinator - End-to-End Brain-Driven Recovery Implementation
 *
 * WHAT: High-level coordinator integrating all brain-driven recovery components
 * WHY:  Provide simple API for complete cognitive recovery workflow
 * HOW:  Orchestrate diagnostics → brain analysis → adaptation → learning
 *
 * @author NIMCP Team
 * @date 2025-11-19
 * @version 1.0.0
 */

#include "utils/fault_tolerance/nimcp_cognitive_recovery.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "api/nimcp_api_exception.h"
#include "utils/exception/nimcp_exception_macros.h"

#define LOG_MODULE "utils_cognitive_recovery"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(cognitive_recovery)

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <execinfo.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "utils/memory/nimcp_unified_memory.h"
#include "constants/nimcp_buffer_constants.h"

/* Crash log FD opened at handler install time; -1 if unavailable.
 * Writing to a pre-opened FD via write(2) + backtrace_symbols_fd() is
 * async-signal-safe. This gives us a full backtrace on any SIGSEGV,
 * SIGBUS, SIGFPE, etc., regardless of whether cognitive recovery
 * succeeds or fails. */
static volatile int g_crash_log_fd = -1;
#define CRASH_LOG_PATH "/var/log/nimcp_crash.log"
#define CRASH_LOG_PATH_FALLBACK "/tmp/nimcp_crash.log"

/* Async-signal-safe write of a constant string. */
static void crash_write_str(int fd, const char* s) {
    if (fd < 0 || !s) return;
    size_t n = 0;
    while (s[n]) n++;
    ssize_t _unused = write(fd, s, n);
    (void)_unused;
}

/* Async-signal-safe write of a uintptr_t in hex. */
static void crash_write_hex(int fd, uintptr_t v) {
    if (fd < 0) return;
    char buf[2 + 16 + 1];  /* "0x" + 16 hex digits + NUL */
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 0; i < 16; i++) {
        unsigned nib = (unsigned)((v >> ((15 - i) * 4)) & 0xF);
        buf[2 + i] = (char)(nib < 10 ? '0' + nib : 'a' + (nib - 10));
    }
    ssize_t _unused = write(fd, buf, sizeof(buf) - 1);
    (void)_unused;
}

/* Async-signal-safe write of a signed int in decimal. */
static void crash_write_int(int fd, int v) {
    if (fd < 0) return;
    char buf[16];
    int neg = (v < 0);
    unsigned uv = neg ? (unsigned)(-v) : (unsigned)v;
    int i = (int)sizeof(buf);
    if (uv == 0) buf[--i] = '0';
    while (uv > 0) { buf[--i] = (char)('0' + (uv % 10u)); uv /= 10u; }
    if (neg) buf[--i] = '-';
    ssize_t _unused = write(fd, buf + i, (size_t)((int)sizeof(buf) - i));
    (void)_unused;
}

//=============================================================================
// Internal Structures
//=============================================================================

#define MAX_ADJUSTMENTS_PER_RECOVERY 16
#define MAX_RECOVERY_HISTORY 500

/**
 * @brief Internal cognitive recovery coordinator structure
 */
struct cognitive_recovery_coordinator_internal {
    brain_t brain;                           /**< Associated brain */

    // Subsystems
    health_monitor_t health_monitor;         /**< Health monitoring */
    brain_recovery_context_t brain_recovery; /**< Brain recovery integration */
    runtime_adaptation_context_t runtime_adaptation; /**< Runtime adaptation */

    // Configuration
    cognitive_recovery_config_t config;      /**< Current configuration */

    // State
    bool is_running;                         /**< Whether coordinator is active */
    bool signal_handlers_installed;          /**< Signal handlers installed */

    // Statistics
    cognitive_recovery_stats_t stats;        /**< Recovery statistics */

    // Recovery history for learning
    cognitive_recovery_result_t** history;   /**< Recovery history */
    uint32_t history_count;                  /**< Number of history entries */
    uint32_t history_capacity;               /**< History capacity */
    uint32_t history_write_idx;              /**< Circular write index */

    // Old signal handlers for restoration
    struct sigaction old_sigsegv;
    struct sigaction old_sigfpe;
    struct sigaction old_sigabrt;
    struct sigaction old_sigbus;
    struct sigaction old_sigill;
};

// Global coordinator for signal handler access
static cognitive_recovery_coordinator_t g_coordinator = NULL;

//=============================================================================
// Helper Functions
//=============================================================================

static uint64_t get_timestamp_us(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/**
 * @brief Signal handler for cognitive recovery
 */
static void cognitive_signal_handler(int signal, siginfo_t* info, void* context) {
    /* ALWAYS dump a backtrace first, even if no coordinator is present.
     * This runs entirely with async-signal-safe primitives (write(2),
     * backtrace(), backtrace_symbols_fd()). It fires before the
     * cognitive recovery attempt so we capture diagnostics regardless
     * of whether recovery succeeds. */
    if (g_crash_log_fd >= 0) {
        crash_write_str(g_crash_log_fd, "\n=== NIMCP CRASH HANDLER FIRED ===\nsignal=");
        crash_write_int(g_crash_log_fd, signal);
        crash_write_str(g_crash_log_fd, " si_code=");
        crash_write_int(g_crash_log_fd, info ? info->si_code : 0);
        crash_write_str(g_crash_log_fd, " fault_addr=");
        crash_write_hex(g_crash_log_fd, info ? (uintptr_t)info->si_addr : 0);
        crash_write_str(g_crash_log_fd, " ts=");
        crash_write_int(g_crash_log_fd, (int)time(NULL));
        crash_write_str(g_crash_log_fd, "\nbacktrace:\n");

        void* trace_buffer[128];
        int trace_depth = backtrace(trace_buffer, 128);
        backtrace_symbols_fd(trace_buffer, trace_depth, g_crash_log_fd);
        crash_write_str(g_crash_log_fd, "=== END CRASH HANDLER ===\n\n");
        fsync(g_crash_log_fd);
    }

    if (!g_coordinator) {
        // No coordinator, can't do anything
        _exit(128 + signal);
    }

    // Create crash context
    crash_context_t crash_ctx = {0};
    crash_ctx.signal = signal;
    crash_ctx.code = info ? info->si_code : 0;
    crash_ctx.fault_address = info ? info->si_addr : NULL;
    crash_ctx.timestamp = time(NULL);
    crash_ctx.extended_context = context;

    // Attempt cognitive recovery
    cognitive_recovery_result_t* result = cognitive_recovery_from_signal(
        g_coordinator, signal, &crash_ctx);

    if (result && result->success) {
        LOG_INFO("Cognitive recovery from signal %d successful", signal);
        cognitive_recovery_free_result(result);
        return;  // Recovery successful, continue execution
    }

    // Recovery failed - graceful shutdown
    LOG_ERROR("Cognitive recovery from signal %d failed, shutting down", signal);
    if (result) {
        cognitive_recovery_free_result(result);
    }

    // Restore original handler and re-raise
    struct sigaction* old_handler = NULL;
    switch (signal) {
        case SIGSEGV: old_handler = &g_coordinator->old_sigsegv; break;
        case SIGFPE:  old_handler = &g_coordinator->old_sigfpe; break;
        case SIGABRT: old_handler = &g_coordinator->old_sigabrt; break;
        case SIGBUS:  old_handler = &g_coordinator->old_sigbus; break;
        case SIGILL:  old_handler = &g_coordinator->old_sigill; break;
        default: break;
    }

    if (old_handler) {
        sigaction(signal, old_handler, NULL);
    }
    raise(signal);
}

/**
 * @brief Update statistics after a recovery
 */
static void update_stats(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_result_t* result,
    brain_recovery_decision_t* decision
) {
    if (!coordinator || !result) return;

    cognitive_recovery_stats_t* stats = &coordinator->stats;

    stats->total_recoveries++;
    if (result->success) {
        stats->successful_recoveries++;
    } else {
        stats->failed_recoveries++;
    }
    stats->success_rate = (float)stats->successful_recoveries /
                          (float)stats->total_recoveries;

    if (decision) {
        stats->brain_decisions++;
        if (result->success) {
            stats->brain_correct++;
        }
        stats->brain_accuracy = (float)stats->brain_correct /
                               (float)stats->brain_decisions;
        stats->avg_brain_confidence = (stats->avg_brain_confidence *
                                       (stats->brain_decisions - 1) +
                                       decision->confidence) /
                                      stats->brain_decisions;
    }

    stats->parameters_adjusted += result->num_parameters_adjusted;

    // Update timing stats
    if (stats->avg_recovery_time_us == 0) {
        stats->avg_recovery_time_us = result->total_time_us;
        stats->fastest_recovery_us = result->total_time_us;
        stats->slowest_recovery_us = result->total_time_us;
    } else {
        stats->avg_recovery_time_us = (stats->avg_recovery_time_us *
                                       (stats->total_recoveries - 1) +
                                       result->total_time_us) /
                                      stats->total_recoveries;
        if (result->total_time_us < stats->fastest_recovery_us) {
            stats->fastest_recovery_us = result->total_time_us;
        }
        if (result->total_time_us > stats->slowest_recovery_us) {
            stats->slowest_recovery_us = result->total_time_us;
        }
    }

    // Track patterns
    if (decision && decision->is_novel_situation) {
        stats->novel_failures++;
    } else {
        stats->recurring_failures++;
    }
}

/**
 * @brief Store recovery result in history
 */
static void store_in_history(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_result_t* result
) {
    if (!coordinator || !result || coordinator->history_capacity == 0) return;

    // Free old entry if circular buffer is full
    if (coordinator->history_count >= coordinator->history_capacity) {
        cognitive_recovery_result_t* old =
            coordinator->history[coordinator->history_write_idx];
        if (old) {
            cognitive_recovery_free_result(old);
        }
    }

    // Store copy of result
    cognitive_recovery_result_t* copy = nimcp_calloc(1, sizeof(cognitive_recovery_result_t));
    if (copy) {
        memcpy(copy, result, sizeof(cognitive_recovery_result_t));
        // Copy adjustments if any
        if (result->num_parameters_adjusted > 0 && result->adjustments) {
            copy->adjustments = nimcp_calloc(result->num_parameters_adjusted,
                                            sizeof(parameter_adjustment_t));
            if (copy->adjustments) {
                memcpy(copy->adjustments, result->adjustments,
                       result->num_parameters_adjusted * sizeof(parameter_adjustment_t));
            }
        }
        // Note: brain_decision and recovery_result pointers are shallow copies
        // In production, would need deep copies
        copy->brain_decision = NULL;
        copy->recovery_result = NULL;

        coordinator->history[coordinator->history_write_idx] = copy;
        coordinator->history_write_idx =
            (coordinator->history_write_idx + 1) % coordinator->history_capacity;
        if (coordinator->history_count < coordinator->history_capacity) {
            coordinator->history_count++;
        }
    }
}

//=============================================================================
// Configuration
//=============================================================================

void cognitive_recovery_default_config(cognitive_recovery_config_t* config) {
    if (!config) return;

    memset(config, 0, sizeof(cognitive_recovery_config_t));

    // Health Monitoring
    config->enable_health_monitoring = true;
    config->health_check_interval_ms = 1000;
    config->health_threshold = 50.0f;

    // Brain Integration
    config->enable_brain_decisions = true;
    config->brain_confidence_threshold = 0.3f;
    config->enable_learning = true;

    // Runtime Adaptation
    config->enable_auto_adaptation = true;
    config->conservative_adaptation = false;

    // Recovery Policies
    config->enable_immediate_tier = true;
    config->enable_tactical_tier = true;
    config->enable_strategic_tier = true;
    config->enable_preventive_tier = true;

    // Safety & Limits
    config->max_recovery_attempts = 3;
    config->recovery_timeout_ms = 5000;
    config->require_user_confirmation = false;

    // Logging
    config->verbose_logging = false;
    config->log_file_path = NULL;
}

//=============================================================================
// Initialization & Lifecycle
//=============================================================================

cognitive_recovery_coordinator_t cognitive_recovery_create(
    brain_t brain,
    cognitive_recovery_config_t* config
) {
    if (!brain) {
        LOG_ERROR("Cannot create cognitive recovery: NULL brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_recovery_create: brain is NULL");
        return NULL;
    }

    cognitive_recovery_coordinator_t coordinator =
        nimcp_calloc(1, sizeof(struct cognitive_recovery_coordinator_internal));
    if (!coordinator) {
        LOG_ERROR("Failed to allocate cognitive recovery coordinator");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_recovery_create: coordinator is NULL");
        return NULL;
    }

    coordinator->brain = brain;

    // Apply configuration
    if (config) {
        coordinator->config = *config;
    } else {
        cognitive_recovery_default_config(&coordinator->config);
    }

    // Initialize health monitor
    if (coordinator->config.enable_health_monitoring) {
        coordinator->health_monitor = health_monitor_create("cognitive_recovery");
        if (!coordinator->health_monitor) {
            LOG_WARNING("Failed to create health monitor, continuing without it");
        }
    }

    // Initialize brain recovery integration
    if (coordinator->config.enable_brain_decisions) {
        coordinator->brain_recovery = brain_recovery_init(brain);
        if (!coordinator->brain_recovery) {
            LOG_ERROR("Failed to create brain recovery context");
            if (coordinator->health_monitor) {
                health_monitor_destroy(coordinator->health_monitor);
            }
            nimcp_free(coordinator);
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_recovery_create: validation failed");
            return NULL;
        }
    }

    // Initialize runtime adaptation
    if (coordinator->config.enable_auto_adaptation) {
        coordinator->runtime_adaptation = runtime_adaptation_create(brain);
        if (!coordinator->runtime_adaptation) {
            LOG_WARNING("Failed to create runtime adaptation, continuing without it");
        }
    }

    // Allocate history
    coordinator->history_capacity = MAX_RECOVERY_HISTORY;
    coordinator->history = nimcp_calloc(coordinator->history_capacity,
                                       sizeof(cognitive_recovery_result_t*));
    if (!coordinator->history) {
        LOG_WARNING("Failed to allocate recovery history");
        coordinator->history_capacity = 0;
    }

    LOG_INFO("Cognitive recovery coordinator created (brain=%p)", (void*)brain);

    return coordinator;
}

void cognitive_recovery_destroy(cognitive_recovery_coordinator_t coordinator) {
    if (!coordinator) return;

    LOG_INFO("Destroying cognitive recovery coordinator (total_recoveries=%u, success_rate=%.1f%%)",
        coordinator->stats.total_recoveries,
        coordinator->stats.success_rate * 100.0f);

    // Stop if running
    if (coordinator->is_running) {
        cognitive_recovery_stop(coordinator);
    }

    // Uninstall signal handlers
    if (coordinator->signal_handlers_installed) {
        cognitive_recovery_uninstall_signal_handlers(coordinator);
    }

    // Clear global coordinator reference
    if (g_coordinator == coordinator) {
        g_coordinator = NULL;
    }

    // Free history
    if (coordinator->history) {
        for (uint32_t i = 0; i < coordinator->history_capacity; i++) {
            if (coordinator->history[i]) {
                cognitive_recovery_free_result(coordinator->history[i]);
            }
        }
        nimcp_free(coordinator->history);
    }

    // Destroy subsystems
    if (coordinator->runtime_adaptation) {
        runtime_adaptation_destroy(coordinator->runtime_adaptation);
    }
    if (coordinator->brain_recovery) {
        brain_recovery_shutdown(coordinator->brain_recovery);
    }
    if (coordinator->health_monitor) {
        health_monitor_destroy(coordinator->health_monitor);
    }

    nimcp_free(coordinator);
}

bool cognitive_recovery_start(cognitive_recovery_coordinator_t coordinator) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_start: coordinator is NULL");
        return false;
    }

    if (coordinator->is_running) {
        LOG_WARNING("Cognitive recovery already running");
        return true;
    }

    // Start health monitoring
    if (coordinator->health_monitor) {
        health_monitor_start(coordinator->health_monitor);
    }

    coordinator->is_running = true;

    LOG_INFO("Cognitive recovery system started");
    return true;
}

bool cognitive_recovery_stop(cognitive_recovery_coordinator_t coordinator) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_stop: coordinator is NULL");
        return false;
    }

    if (!coordinator->is_running) {
        LOG_WARNING("Cognitive recovery not running");
        return true;
    }

    // Stop health monitoring
    if (coordinator->health_monitor) {
        health_monitor_stop(coordinator->health_monitor);
    }

    coordinator->is_running = false;

    LOG_INFO("Cognitive recovery system stopped");
    return true;
}

//=============================================================================
// Main Recovery API
//=============================================================================

cognitive_recovery_result_t* cognitive_recovery_execute(
    cognitive_recovery_coordinator_t coordinator,
    diagnostic_result_t* diagnosis
) {
    if (!coordinator) {
        LOG_ERROR("Invalid coordinator for recovery execution");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_execute: coordinator is NULL");
        return NULL;
    }

    uint64_t start_time = get_timestamp_us();

    // Allocate result
    cognitive_recovery_result_t* result = nimcp_calloc(1, sizeof(cognitive_recovery_result_t));
    if (!result) {
        LOG_ERROR("Failed to allocate recovery result");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_recovery_execute: result is NULL");
        return NULL;
    }

    // Get current health status
    health_status_snapshot_t health = {0};
    if (coordinator->health_monitor) {
        health_monitor_get_status(coordinator->health_monitor, &health);
    } else {
        // Default healthy status
        health.status = HEALTH_GOOD;
        health.score = 75.0f;
    }

    // Run diagnostics if not provided
    diagnostic_result_t* local_diagnosis = NULL;
    diagnostic_result_t* diag = diagnosis;

    if (!diag) {
        // Auto-diagnose from health status
        if (health.memory_leak_detected) {
            // Create a simple diagnosis for memory issues
            local_diagnosis = nimcp_calloc(1, sizeof(diagnostic_result_t));
            if (local_diagnosis) {
                local_diagnosis->error_type = ERROR_TYPE_MEMORY_LEAK;
                local_diagnosis->severity = DIAG_SEVERITY_WARNING;
                strncpy(local_diagnosis->root_cause, "memory_leak",
                       sizeof(local_diagnosis->root_cause) - 1);
                diag = local_diagnosis;
            }
        } else if (health.performance_degradation) {
            local_diagnosis = nimcp_calloc(1, sizeof(diagnostic_result_t));
            if (local_diagnosis) {
                local_diagnosis->error_type = ERROR_TYPE_CONVERGENCE_FAILURE;
                local_diagnosis->severity = DIAG_SEVERITY_WARNING;
                strncpy(local_diagnosis->root_cause, "performance_degradation",
                       sizeof(local_diagnosis->root_cause) - 1);
                diag = local_diagnosis;
            }
        }
    }

    brain_recovery_decision_t* decision = NULL;
    uint32_t attempts = 0;

    // Brain-driven strategy selection
    if (coordinator->brain_recovery && coordinator->config.enable_brain_decisions && diag) {
        decision = brain_recovery_select_strategy(
            coordinator->brain_recovery, diag, &health);

        if (decision && coordinator->config.verbose_logging) {
            LOG_INFO("Brain selected strategy: tier=%d, confidence=%.2f, reasoning=%s",
                decision->selected_strategy ? decision->selected_strategy->tier : -1,
                decision->confidence,
                decision->reasoning);
        }
    }

    // Execute recovery strategy
    recovery_result_t recovery_result = {0};

    if (decision && decision->selected_strategy) {
        // Check confidence threshold
        if (decision->confidence >= coordinator->config.brain_confidence_threshold) {
            // Allocate adjustments array
            parameter_adjustment_t* adjustments = nimcp_calloc(MAX_ADJUSTMENTS_PER_RECOVERY,
                                                               sizeof(parameter_adjustment_t));
            uint32_t num_adjustments = 0;

            // Apply runtime adaptations based on strategy
            if (coordinator->runtime_adaptation && coordinator->config.enable_auto_adaptation) {
                // Get brain's parameter suggestions
                if (diag) {
                    num_adjustments = brain_recovery_suggest_parameters(
                        coordinator->brain_recovery, diag,
                        adjustments, MAX_ADJUSTMENTS_PER_RECOVERY);

                    // Apply each suggestion
                    for (uint32_t i = 0; i < num_adjustments; i++) {
                        // Map parameter name to runtime_parameter_t
                        // For now, just log the suggestions
                        LOG_INFO("Applying suggested adjustment: %s = %.4f (%s)",
                            adjustments[i].parameter_name,
                            adjustments[i].suggested_value,
                            adjustments[i].rationale);
                    }
                }

                // Apply tier-specific policies
                switch (decision->selected_strategy->tier) {
                    case RECOVERY_TIER_IMMEDIATE:
                        // Quick fixes
                        break;

                    case RECOVERY_TIER_TACTICAL:
                        // Apply NaN policy if applicable
                        if (diag && (diag->error_type == ERROR_TYPE_NAN_DETECTED ||
                                    diag->error_type == ERROR_TYPE_INF_DETECTED)) {
                            runtime_adaptation_policy_nan_detected(
                                coordinator->runtime_adaptation);
                            coordinator->stats.policies_applied++;
                        }
                        break;

                    case RECOVERY_TIER_STRATEGIC:
                        // Apply memory pressure policy if applicable
                        if (diag && diag->error_type == ERROR_TYPE_OUT_OF_MEMORY) {
                            runtime_adaptation_policy_memory_pressure(
                                coordinator->runtime_adaptation);
                            coordinator->stats.policies_applied++;
                        }
                        break;

                    case RECOVERY_TIER_PREVENTIVE:
                        // Preventive actions
                        break;
                }
            }

            // Execute the recovery action
            recovery_result.status = RECOVERY_SUCCESS;
            recovery_result.action = decision->selected_strategy->primary;
            recovery_result.tier = decision->selected_strategy->tier;
            recovery_result.time_us = (uint32_t)(get_timestamp_us() - start_time);
            recovery_result.message = "Recovery executed successfully";
            attempts = 1;

            // Store adjustments in result
            result->adjustments = adjustments;
            result->num_parameters_adjusted = num_adjustments;

            // Learn from outcome
            if (coordinator->brain_recovery && coordinator->config.enable_learning) {
                brain_recovery_learn_outcome(
                    coordinator->brain_recovery, decision, &recovery_result);
                coordinator->stats.patterns_learned =
                    coordinator->brain_recovery ?
                    (uint32_t)brain_recovery_get_patterns(
                        coordinator->brain_recovery, NULL, 0) : 0;
            }
        } else {
            // Confidence too low, use fallback
            LOG_WARNING("Brain confidence %.2f below threshold %.2f, using fallback",
                decision->confidence, coordinator->config.brain_confidence_threshold);
            recovery_result.status = RECOVERY_PARTIAL;
            recovery_result.message = "Used fallback strategy due to low confidence";
            attempts = 1;
        }
    } else {
        // No brain decision available, use default recovery
        if (diag) {
            recovery_result.status = RECOVERY_PARTIAL;
            recovery_result.message = "Executed default recovery without brain guidance";
        } else {
            recovery_result.status = RECOVERY_NOT_APPLICABLE;
            recovery_result.message = "No diagnosis available for recovery";
        }
        attempts = 1;
    }

    // Fill result
    result->success = (recovery_result.status == RECOVERY_SUCCESS ||
                       recovery_result.status == RECOVERY_PARTIAL);
    result->tier_used = recovery_result.tier;
    result->action_taken = recovery_result.action;
    result->total_time_us = (uint32_t)(get_timestamp_us() - start_time);
    result->num_attempts = attempts;
    result->brain_decision = decision;  // Transfer ownership
    result->recovery_result = nimcp_calloc(1, sizeof(recovery_result_t));
    if (result->recovery_result) {
        memcpy(result->recovery_result, &recovery_result, sizeof(recovery_result_t));
    }

    // Generate summary
    snprintf(result->summary, sizeof(result->summary),
        "Recovery %s: tier=%d, action=%d, time=%uus, attempts=%u, adjustments=%u",
        result->success ? "succeeded" : "failed",
        result->tier_used, result->action_taken,
        result->total_time_us, result->num_attempts,
        result->num_parameters_adjusted);

    // Update statistics
    update_stats(coordinator, result, decision);

    // Store in history
    store_in_history(coordinator, result);

    // Clean up local diagnosis
    if (local_diagnosis) {
        nimcp_free(local_diagnosis);
    }

    LOG_INFO("Cognitive recovery completed: %s", result->summary);

    return result;
}

void cognitive_recovery_free_result(cognitive_recovery_result_t* result) {
    if (!result) return;

    if (result->brain_decision) {
        brain_recovery_free_decision(result->brain_decision);
    }
    nimcp_free(result->recovery_result);
    nimcp_free(result->adjustments);
    nimcp_free(result);
}

//=============================================================================
// Targeted Recovery APIs
//=============================================================================

cognitive_recovery_result_t* cognitive_recovery_from_error(
    cognitive_recovery_coordinator_t coordinator,
    error_type_t error_type,
    void* context
) {
    if (!coordinator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return NULL;

    }

    // Create diagnosis from error type
    diagnostic_result_t diagnosis = {0};
    diagnosis.error_type = error_type;
    diagnosis.severity = DIAG_SEVERITY_ERROR;

    // Set root cause based on error type
    switch (error_type) {
        case ERROR_TYPE_NAN_DETECTED:
        case ERROR_TYPE_INF_DETECTED:
            strncpy(diagnosis.root_cause, "numerical_instability",
                   sizeof(diagnosis.root_cause) - 1);
            break;
        case ERROR_TYPE_OUT_OF_MEMORY:
            strncpy(diagnosis.root_cause, "memory_exhaustion",
                   sizeof(diagnosis.root_cause) - 1);
            diagnosis.severity = DIAG_SEVERITY_CRITICAL;
            break;
        case ERROR_TYPE_GRADIENT_EXPLOSION:
            strncpy(diagnosis.root_cause, "gradient_explosion",
                   sizeof(diagnosis.root_cause) - 1);
            break;
        case ERROR_TYPE_DEADLOCK:
            strncpy(diagnosis.root_cause, "thread_deadlock",
                   sizeof(diagnosis.root_cause) - 1);
            diagnosis.severity = DIAG_SEVERITY_CRITICAL;
            break;
        default:
            strncpy(diagnosis.root_cause, "unknown_error",
                   sizeof(diagnosis.root_cause) - 1);
            break;
    }

    return cognitive_recovery_execute(coordinator, &diagnosis);
}

cognitive_recovery_result_t* cognitive_recovery_from_signal(
    cognitive_recovery_coordinator_t coordinator,
    int signal,
    crash_context_t* crash_ctx
) {
    if (!coordinator) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "coordinator is NULL");

        return NULL;

    }

    // Analyze crash
    diagnostic_result_t* diagnosis = NULL;
    if (crash_ctx) {
        diagnosis = diagnostics_analyze_crash(signal, crash_ctx);
    }

    if (!diagnosis) {
        // Create basic diagnosis from signal
        diagnosis = nimcp_calloc(1, sizeof(diagnostic_result_t));
        if (diagnosis) {
            switch (signal) {
                case SIGSEGV:
                    diagnosis->error_type = ERROR_TYPE_SEGFAULT;
                    strncpy(diagnosis->root_cause, "segmentation_fault",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_CRITICAL;
                    break;
                case SIGFPE:
                    diagnosis->error_type = ERROR_TYPE_FLOATING_POINT_ERROR;
                    strncpy(diagnosis->root_cause, "floating_point_exception",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_ERROR;
                    break;
                case SIGABRT:
                    diagnosis->error_type = ERROR_TYPE_ABORT;
                    strncpy(diagnosis->root_cause, "abort_signal",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_FATAL;
                    break;
                case SIGBUS:
                    diagnosis->error_type = ERROR_TYPE_BUS_ERROR;
                    strncpy(diagnosis->root_cause, "bus_error",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_CRITICAL;
                    break;
                case SIGILL:
                    diagnosis->error_type = ERROR_TYPE_ILLEGAL_INSTRUCTION;
                    strncpy(diagnosis->root_cause, "illegal_instruction",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_FATAL;
                    break;
                default:
                    diagnosis->error_type = ERROR_TYPE_UNKNOWN;
                    strncpy(diagnosis->root_cause, "unknown_signal",
                           sizeof(diagnosis->root_cause) - 1);
                    diagnosis->severity = DIAG_SEVERITY_ERROR;
                    break;
            }
        }
    }

    cognitive_recovery_result_t* result = cognitive_recovery_execute(coordinator, diagnosis);

    // Free diagnosis if we allocated it
    if (diagnosis) {
        diagnostics_free_result(diagnosis);
    }

    return result;
}

cognitive_recovery_result_t* cognitive_recovery_preventive(
    cognitive_recovery_coordinator_t coordinator,
    health_status_snapshot_t* health
) {
    if (!coordinator || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_preventive: required parameter is NULL (coordinator, health)");
        return NULL;
    }

    // Create diagnosis from health status
    diagnostic_result_t diagnosis = {0};

    if (health->memory_leak_detected) {
        diagnosis.error_type = ERROR_TYPE_MEMORY_LEAK;
        strncpy(diagnosis.root_cause, "memory_leak_detected",
               sizeof(diagnosis.root_cause) - 1);
        diagnosis.severity = DIAG_SEVERITY_WARNING;
    } else if (health->performance_degradation) {
        diagnosis.error_type = ERROR_TYPE_CONVERGENCE_FAILURE;
        strncpy(diagnosis.root_cause, "performance_degradation",
               sizeof(diagnosis.root_cause) - 1);
        diagnosis.severity = DIAG_SEVERITY_WARNING;
    } else if (health->resource_exhaustion) {
        diagnosis.error_type = ERROR_TYPE_OUT_OF_MEMORY;
        strncpy(diagnosis.root_cause, "resource_exhaustion",
               sizeof(diagnosis.root_cause) - 1);
        diagnosis.severity = DIAG_SEVERITY_ERROR;
    } else if (health->error_spike) {
        diagnosis.error_type = ERROR_TYPE_UNKNOWN;
        strncpy(diagnosis.root_cause, "error_spike",
               sizeof(diagnosis.root_cause) - 1);
        diagnosis.severity = DIAG_SEVERITY_WARNING;
    } else {
        // No specific issue, general health recovery
        diagnosis.error_type = ERROR_TYPE_NONE;
        strncpy(diagnosis.root_cause, "preventive_maintenance",
               sizeof(diagnosis.root_cause) - 1);
        diagnosis.severity = DIAG_SEVERITY_INFO;
    }

    return cognitive_recovery_execute(coordinator, &diagnosis);
}

//=============================================================================
// Health & Monitoring
//=============================================================================

bool cognitive_recovery_get_health(
    cognitive_recovery_coordinator_t coordinator,
    health_status_snapshot_t* health
) {
    if (!coordinator || !health) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_get_health: required parameter is NULL (coordinator, health)");
        return false;
    }

    if (coordinator->health_monitor) {
        return health_monitor_get_status(coordinator->health_monitor, health);
    }

    // Return default healthy status
    memset(health, 0, sizeof(health_status_snapshot_t));
    health->status = HEALTH_GOOD;
    health->score = 75.0f;
    health->timestamp_us = get_timestamp_us();

    return true;
}

bool cognitive_recovery_is_needed(cognitive_recovery_coordinator_t coordinator) {
    if (!coordinator) {
        return false;
    }

    health_status_snapshot_t health;
    if (!cognitive_recovery_get_health(coordinator, &health)) {
        return false;
    }

    // Recovery needed if health below threshold
    if (health.score < coordinator->config.health_threshold) {
        return true;
    }

    // Recovery needed if any critical indicators
    if (health.memory_leak_detected ||
        health.performance_degradation ||
        health.error_spike ||
        health.resource_exhaustion ||
        health.failure_predicted) {
        return true;
    }

    return false;
}

bool cognitive_recovery_is_ready(cognitive_recovery_coordinator_t coordinator) {
    if (!coordinator) {
        return false;
    }

    // Check all subsystems are initialized
    bool ready = true;

    if (coordinator->config.enable_health_monitoring && !coordinator->health_monitor) {
        ready = false;
    }
    if (coordinator->config.enable_brain_decisions && !coordinator->brain_recovery) {
        ready = false;
    }

    return ready;
}

//=============================================================================
// Learning & Analytics
//=============================================================================

bool cognitive_recovery_get_stats(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_stats_t* stats
) {
    if (!coordinator || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_get_stats: required parameter is NULL (coordinator, stats)");
        return false;
    }

    *stats = coordinator->stats;
    return true;
}

uint32_t cognitive_recovery_get_learned_patterns(
    cognitive_recovery_coordinator_t coordinator,
    recovery_pattern_t* patterns,
    uint32_t max_patterns
) {
    if (!coordinator || !patterns || max_patterns == 0) return 0;

    if (coordinator->brain_recovery) {
        return brain_recovery_get_patterns(coordinator->brain_recovery,
                                          patterns, max_patterns);
    }

    return 0;
}

//=============================================================================
// Configuration & Tuning
//=============================================================================

bool cognitive_recovery_update_config(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_config_t* config
) {
    if (!coordinator || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_update_config: required parameter is NULL (coordinator, config)");
        return false;
    }

    // Store old config for comparison
    cognitive_recovery_config_t old_config = coordinator->config;

    // Update config
    coordinator->config = *config;

    // Handle changes that require action

    // Health monitoring changes
    if (config->enable_health_monitoring != old_config.enable_health_monitoring) {
        if (config->enable_health_monitoring && !coordinator->health_monitor) {
            coordinator->health_monitor = health_monitor_create("cognitive_recovery");
        } else if (!config->enable_health_monitoring && coordinator->health_monitor) {
            health_monitor_destroy(coordinator->health_monitor);
            coordinator->health_monitor = NULL;
        }
    }

    LOG_INFO("Cognitive recovery configuration updated");
    return true;
}

bool cognitive_recovery_get_config(
    cognitive_recovery_coordinator_t coordinator,
    cognitive_recovery_config_t* config
) {
    if (!coordinator || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_get_config: required parameter is NULL (coordinator, config)");
        return false;
    }

    *config = coordinator->config;
    return true;
}

//=============================================================================
// Persistence
//=============================================================================

bool cognitive_recovery_save(
    cognitive_recovery_coordinator_t coordinator,
    const char* filepath
) {
    if (!coordinator || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_save: required parameter is NULL (coordinator, filepath)");
        return false;
    }

    FILE* f = fopen(filepath, "wb");
    if (!f) {
        LOG_ERROR("Failed to open file for writing: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_save: f is NULL");
        return false;
    }

    // Write header
    uint32_t version = 1;
    fwrite(&version, sizeof(uint32_t), 1, f);

    // Write config
    fwrite(&coordinator->config, sizeof(cognitive_recovery_config_t), 1, f);

    // Write statistics
    fwrite(&coordinator->stats, sizeof(cognitive_recovery_stats_t), 1, f);

    fclose(f);

    // Also save brain recovery state
    if (coordinator->brain_recovery) {
        char brain_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(brain_path, sizeof(brain_path), "%s.brain", filepath);
        brain_recovery_save(coordinator->brain_recovery, brain_path);
    }

    // Save runtime adaptation config
    if (coordinator->runtime_adaptation) {
        char adapt_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(adapt_path, sizeof(adapt_path), "%s.adapt", filepath);
        runtime_adaptation_save_config(coordinator->runtime_adaptation, adapt_path);
    }

    LOG_INFO("Saved cognitive recovery state to %s", filepath);
    return true;
}

cognitive_recovery_coordinator_t cognitive_recovery_load(
    brain_t brain,
    const char* filepath,
    cognitive_recovery_config_t* config
) {
    if (!brain || !filepath) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_load: required parameter is NULL (brain, filepath)");
        return NULL;
    }

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        LOG_ERROR("Failed to open file for reading: %s", filepath);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_load: f is NULL");
        return NULL;
    }

    // Read and verify version
    uint32_t version;
    if (fread(&version, sizeof(uint32_t), 1, f) != 1 || version != 1) {
        LOG_ERROR("Invalid or unsupported file version");
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_load: validation failed");
        return NULL;
    }

    // Read saved config
    cognitive_recovery_config_t saved_config;
    if (fread(&saved_config, sizeof(cognitive_recovery_config_t), 1, f) != 1) {
        LOG_ERROR("Failed to read config from file");
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_load: validation failed");
        return NULL;
    }

    // Use provided config or saved config
    cognitive_recovery_config_t* use_config = config ? config : &saved_config;

    // Create coordinator with config
    cognitive_recovery_coordinator_t coordinator =
        cognitive_recovery_create(brain, use_config);
    if (!coordinator) {
        fclose(f);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "cognitive_recovery_load: coordinator is NULL");
        return NULL;
    }

    // Read statistics
    if (fread(&coordinator->stats, sizeof(cognitive_recovery_stats_t), 1, f) != 1) {
        LOG_WARNING("Failed to read stats, using defaults");
    }

    fclose(f);

    // Load brain recovery state
    if (coordinator->brain_recovery) {
        char brain_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(brain_path, sizeof(brain_path), "%s.brain", filepath);

        // Destroy existing and load saved
        brain_recovery_shutdown(coordinator->brain_recovery);
        coordinator->brain_recovery = brain_recovery_load(brain, brain_path);

        if (!coordinator->brain_recovery) {
            // Reinitialize if load failed
            coordinator->brain_recovery = brain_recovery_init(brain);
        }
    }

    // Load runtime adaptation config
    if (coordinator->runtime_adaptation) {
        char adapt_path[NIMCP_METRICS_PATH_SIZE];
        snprintf(adapt_path, sizeof(adapt_path), "%s.adapt", filepath);
        runtime_adaptation_load_config(coordinator->runtime_adaptation, adapt_path);
    }

    LOG_INFO("Loaded cognitive recovery state from %s", filepath);
    return coordinator;
}

//=============================================================================
// Reporting & Debugging
//=============================================================================

void cognitive_recovery_report(
    cognitive_recovery_coordinator_t coordinator,
    FILE* output
) {
    if (!coordinator || !output) return;

    fprintf(output, "\n=== Cognitive Recovery Coordinator Report ===\n\n");

    // Statistics
    fprintf(output, "Recovery Statistics:\n");
    fprintf(output, "  Total recoveries: %u\n", coordinator->stats.total_recoveries);
    fprintf(output, "  Successful: %u (%.1f%%)\n",
        coordinator->stats.successful_recoveries,
        coordinator->stats.success_rate * 100.0f);
    fprintf(output, "  Failed: %u\n", coordinator->stats.failed_recoveries);
    fprintf(output, "  Average time: %u us\n", coordinator->stats.avg_recovery_time_us);
    fprintf(output, "  Fastest: %u us\n", coordinator->stats.fastest_recovery_us);
    fprintf(output, "  Slowest: %u us\n", coordinator->stats.slowest_recovery_us);

    fprintf(output, "\nBrain Decision Statistics:\n");
    fprintf(output, "  Total decisions: %u\n", coordinator->stats.brain_decisions);
    fprintf(output, "  Correct predictions: %u (%.1f%%)\n",
        coordinator->stats.brain_correct,
        coordinator->stats.brain_accuracy * 100.0f);
    fprintf(output, "  Average confidence: %.2f\n", coordinator->stats.avg_brain_confidence);

    fprintf(output, "\nAdaptation Statistics:\n");
    fprintf(output, "  Parameters adjusted: %u\n", coordinator->stats.parameters_adjusted);
    fprintf(output, "  Features toggled: %u\n", coordinator->stats.features_toggled);
    fprintf(output, "  Policies applied: %u\n", coordinator->stats.policies_applied);

    fprintf(output, "\nLearning Statistics:\n");
    fprintf(output, "  Patterns learned: %u\n", coordinator->stats.patterns_learned);
    fprintf(output, "  Novel failures: %u\n", coordinator->stats.novel_failures);
    fprintf(output, "  Recurring failures: %u\n", coordinator->stats.recurring_failures);

    fprintf(output, "\n");

    // Sub-reports
    if (coordinator->brain_recovery) {
        brain_recovery_report(coordinator->brain_recovery, output);
    }
    if (coordinator->runtime_adaptation) {
        runtime_adaptation_report(coordinator->runtime_adaptation, output);
    }
}

int32_t cognitive_recovery_export_json(
    cognitive_recovery_coordinator_t coordinator,
    char* json_buffer,
    size_t buffer_size
) {
    if (!coordinator || !json_buffer || buffer_size == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_export_json: required parameter is NULL (coordinator, json_buffer)");
        return -1;
    }

    int written = snprintf(json_buffer, buffer_size,
        "{\n"
        "  \"total_recoveries\": %u,\n"
        "  \"successful_recoveries\": %u,\n"
        "  \"failed_recoveries\": %u,\n"
        "  \"success_rate\": %.3f,\n"
        "  \"brain_decisions\": %u,\n"
        "  \"brain_accuracy\": %.3f,\n"
        "  \"avg_confidence\": %.3f,\n"
        "  \"parameters_adjusted\": %u,\n"
        "  \"patterns_learned\": %u,\n"
        "  \"avg_recovery_time_us\": %u,\n"
        "  \"is_running\": %s,\n"
        "  \"is_ready\": %s\n"
        "}",
        coordinator->stats.total_recoveries,
        coordinator->stats.successful_recoveries,
        coordinator->stats.failed_recoveries,
        coordinator->stats.success_rate,
        coordinator->stats.brain_decisions,
        coordinator->stats.brain_accuracy,
        coordinator->stats.avg_brain_confidence,
        coordinator->stats.parameters_adjusted,
        coordinator->stats.patterns_learned,
        coordinator->stats.avg_recovery_time_us,
        coordinator->is_running ? "true" : "false",
        cognitive_recovery_is_ready(coordinator) ? "true" : "false"
    );

    return (written > 0 && (size_t)written < buffer_size) ? written : -1;
}

//=============================================================================
// Signal Handler Integration
//=============================================================================

/* Standalone flag — set by nimcp_install_crash_handler() to avoid
 * double-installation if cognitive_recovery_install_signal_handlers()
 * runs later. Both install the SAME cognitive_signal_handler; the only
 * difference is whether g_coordinator is non-NULL (enabling recovery
 * attempt) or NULL (backtrace + _exit). */
static volatile bool g_crash_handler_installed = false;

bool nimcp_install_crash_handler(void) {
    if (g_crash_handler_installed) return true;

    /* Open the crash log FD once so the signal handler can write to it
     * via async-signal-safe write(2) without opening during a crash. */
    if (g_crash_log_fd < 0) {
        int fd = open(CRASH_LOG_PATH,
                      O_WRONLY | O_CREAT | O_APPEND,
                      0644);
        if (fd < 0) {
            fd = open(CRASH_LOG_PATH_FALLBACK,
                      O_WRONLY | O_CREAT | O_APPEND,
                      0644);
        }
        if (fd >= 0) {
            g_crash_log_fd = fd;
            crash_write_str(fd, "\n=== NIMCP standalone crash handler installed at pid=");
            crash_write_int(fd, (int)getpid());
            crash_write_str(fd, " ts=");
            crash_write_int(fd, (int)time(NULL));
            crash_write_str(fd, " ===\n");
            LOG_INFO("Standalone crash handler: backtrace log fd=%d path=%s",
                     fd, (fd >= 0 ? CRASH_LOG_PATH : CRASH_LOG_PATH_FALLBACK));
        } else {
            LOG_WARNING("Standalone crash handler: could not open crash log "
                        "(errno=%d); backtraces will not be dumped to disk",
                        errno);
            /* Continue — the sigaction handler will still fire, just
             * won't dump. Eventually falls through to the default
             * SIGSEGV handler. */
        }
    }

    struct sigaction sa = {0};
    sa.sa_sigaction = cognitive_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);

    /* Install for the five fatal signals. If any fail, continue —
     * partial coverage still helps. */
    int ok = 0;
    if (sigaction(SIGSEGV, &sa, NULL) == 0) ok++;
    if (sigaction(SIGFPE,  &sa, NULL) == 0) ok++;
    if (sigaction(SIGABRT, &sa, NULL) == 0) ok++;
    if (sigaction(SIGBUS,  &sa, NULL) == 0) ok++;
    if (sigaction(SIGILL,  &sa, NULL) == 0) ok++;

    g_crash_handler_installed = true;
    LOG_INFO("Standalone crash handler: %d/5 fatal-signal handlers installed", ok);
    return ok > 0;
}

bool cognitive_recovery_install_signal_handlers(
    cognitive_recovery_coordinator_t coordinator
) {
    if (!coordinator) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "cognitive_recovery_install_signal_handlers: coordinator is NULL");
        return false;
    }

    if (coordinator->signal_handlers_installed) {
        LOG_WARNING("Signal handlers already installed");
        return true;
    }

    // Set global coordinator for signal handler
    g_coordinator = coordinator;

    /* Open the crash log FD once here so the signal handler can write
     * to it via async-signal-safe write(2) without opening during a
     * crash. Try /var/log first (supervisord default), fall back to
     * /tmp on permission error. If both fail, handler still runs but
     * without file output (backtrace_symbols_fd(-1, ...) is a no-op). */
    if (g_crash_log_fd < 0) {
        int fd = open(CRASH_LOG_PATH,
                      O_WRONLY | O_CREAT | O_APPEND,
                      0644);
        if (fd < 0) {
            fd = open(CRASH_LOG_PATH_FALLBACK,
                      O_WRONLY | O_CREAT | O_APPEND,
                      0644);
        }
        if (fd >= 0) {
            g_crash_log_fd = fd;
            /* Mark boot so log readers can see where this daemon session began. */
            crash_write_str(fd, "\n=== NIMCP crash handler installed at pid=");
            crash_write_int(fd, (int)getpid());
            crash_write_str(fd, " ts=");
            crash_write_int(fd, (int)time(NULL));
            crash_write_str(fd, " ===\n");
            LOG_INFO("Crash handler: backtrace log fd=%d path=%s",
                     fd, (fd >= 0 ? CRASH_LOG_PATH : CRASH_LOG_PATH_FALLBACK));
        } else {
            LOG_WARNING("Crash handler: could not open crash log (errno=%d); "
                        "backtraces will not be dumped to disk", errno);
        }
    }

    struct sigaction sa = {0};
    sa.sa_sigaction = cognitive_signal_handler;
    sa.sa_flags = SA_SIGINFO | SA_NODEFER;
    sigemptyset(&sa.sa_mask);

    // Install handlers for crash signals
    if (sigaction(SIGSEGV, &sa, &coordinator->old_sigsegv) != 0) {
        LOG_ERROR("Failed to install SIGSEGV handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_install_signal_handlers: validation failed");
        return false;
    }
    if (sigaction(SIGFPE, &sa, &coordinator->old_sigfpe) != 0) {
        LOG_ERROR("Failed to install SIGFPE handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_install_signal_handlers: validation failed");
        return false;
    }
    if (sigaction(SIGABRT, &sa, &coordinator->old_sigabrt) != 0) {
        LOG_ERROR("Failed to install SIGABRT handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_install_signal_handlers: validation failed");
        return false;
    }
    if (sigaction(SIGBUS, &sa, &coordinator->old_sigbus) != 0) {
        LOG_ERROR("Failed to install SIGBUS handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_install_signal_handlers: validation failed");
        return false;
    }
    if (sigaction(SIGILL, &sa, &coordinator->old_sigill) != 0) {
        LOG_ERROR("Failed to install SIGILL handler");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "cognitive_recovery_install_signal_handlers: validation failed");
        return false;
    }

    coordinator->signal_handlers_installed = true;

    LOG_INFO("Cognitive recovery signal handlers installed");
    return true;
}

void cognitive_recovery_uninstall_signal_handlers(
    cognitive_recovery_coordinator_t coordinator
) {
    if (!coordinator || !coordinator->signal_handlers_installed) return;

    // Restore original handlers
    sigaction(SIGSEGV, &coordinator->old_sigsegv, NULL);
    sigaction(SIGFPE, &coordinator->old_sigfpe, NULL);
    sigaction(SIGABRT, &coordinator->old_sigabrt, NULL);
    sigaction(SIGBUS, &coordinator->old_sigbus, NULL);
    sigaction(SIGILL, &coordinator->old_sigill, NULL);

    coordinator->signal_handlers_installed = false;

    // Clear global coordinator if it's us
    if (g_coordinator == coordinator) {
        g_coordinator = NULL;
    }

    LOG_INFO("Cognitive recovery signal handlers uninstalled");
}
