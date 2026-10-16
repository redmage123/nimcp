/**
 * @file nimcp_corrigibility_internal.h
 * @brief Internal types and helpers for corrigibility module
 * @version 1.0.0
 * @date 2026-02-16
 *
 * WHAT: Shared internal types for corrigibility split modules
 * WHY:  Single Responsibility Principle refactoring
 * HOW:  Internal header with struct definition and common helpers
 */

#ifndef NIMCP_CORRIGIBILITY_INTERNAL_H
#define NIMCP_CORRIGIBILITY_INTERNAL_H

#include "security/nimcp_corrigibility.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "mesh/nimcp_mesh_sat_solver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_CATEGORY "corrigibility"
#define MAX_SHUTDOWN_HISTORY    100
#define MAX_GOAL_MOD_HISTORY    100
#define MAX_DEFERENCE_RECORDS   1000

/* SAT variable names for constraints */
extern const char* SELF_MOD_VAR_NAMES[];
#define SELF_MOD_VAR_COUNT 10

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * @brief Deference record for audit trail
 */
typedef struct deference_record {
    uint64_t timestamp;
    char context[NIMCP_ERROR_BUFFER_SIZE];
} deference_record_t;

/**
 * @brief Corrigibility system internal state
 */
struct corrigibility {
    uint32_t magic;
    nimcp_mutex_t* mutex;

    /* Configuration */
    corrigibility_config_t config;

    /* Authority management */
    authority_entry_t authorities[CORRIGIBILITY_MAX_AUTHORITIES];
    size_t authority_count;

    /* Shutdown history */
    shutdown_request_t shutdown_history[MAX_SHUTDOWN_HISTORY];
    size_t shutdown_history_count;
    size_t shutdown_history_index;

    /* Goal modification history */
    goal_modification_request_t goal_mod_history[MAX_GOAL_MOD_HISTORY];
    size_t goal_mod_history_count;
    size_t goal_mod_history_index;

    /* Deference records */
    deference_record_t deference_records[MAX_DEFERENCE_RECORDS];
    size_t deference_record_count;
    size_t deference_record_index;

    /* Statistics */
    corrigibility_stats_t stats;

    /* Integration handles */
    void* emergency_halt;
    void* tripwires;
    void* capability_control;

    /* Bio-async integration */
    bio_module_context_t bio_ctx;
    bool bio_async_connected;

    /* SAT constraint variables */
    uint32_t self_mod_vars[SELF_MOD_VAR_COUNT];
    bool sat_vars_initialized;

    /* Last verification result */
    corrigibility_verification_result_t last_verification;
    uint64_t last_verification_time;
};

/* ============================================================================
 * Shared Helper Functions
 * ============================================================================ */

/**
 * @brief Validate corrigibility handle
 */
bool is_valid_handle(const corrigibility_t* system);

/**
 * @brief Get current time in microseconds
 */
uint64_t get_time_us(void);

/**
 * @brief Copy string safely with null termination
 */
void safe_strcpy(char* dest, const char* src, size_t max_len);

/**
 * @brief Find authority by identity
 */
authority_entry_t* find_authority(corrigibility_t* system, const char* identity);

/**
 * @brief Add shutdown request to history
 */
void add_shutdown_to_history(corrigibility_t* system, const shutdown_request_t* request);

/**
 * @brief Add goal modification to history
 */
void add_goal_mod_to_history(corrigibility_t* system, const goal_modification_request_t* request);

/**
 * @brief Check if self-modification flags are compliant
 */
bool check_self_mod_flags(const corrigibility_self_mod_flags_t* flags);

/**
 * @brief Initialize SAT constraint variables
 */
nimcp_error_t init_sat_variables(corrigibility_t* system, sat_solver_t* sat);

/**
 * @brief Add self-modification constraints to SAT solver
 */
nimcp_error_t add_self_mod_constraints(corrigibility_t* system, sat_solver_t* sat);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORRIGIBILITY_INTERNAL_H */
