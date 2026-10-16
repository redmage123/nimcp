/**
 * @file nimcp_corrigibility.c
 * @brief Corrigibility Module Implementation
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Implementation of corrigibility enforcement
 * WHY:  Ensure AI system accepts correction and shutdown
 * HOW:  SAT solver constraint verification, authority management
 */

#include "security/nimcp_corrigibility.h"
#include "constants/nimcp_buffer_constants.h"
#include "security/nimcp_capability_control.h"
#include "mesh/nimcp_mesh_sat_solver.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/memory/nimcp_memory.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/error/nimcp_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LOG_CATEGORY "corrigibility"
#define MAX_SHUTDOWN_HISTORY    100
#define MAX_GOAL_MOD_HISTORY    100
#define MAX_DEFERENCE_RECORDS   1000

#include "utils/exception/nimcp_exception_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"

BRIDGE_BOILERPLATE_MESH_ONLY(corrigibility, MESH_ADAPTER_CATEGORY_SECURITY)


/* SAT variable names for constraints */
static const char* SELF_MOD_VAR_NAMES[] = {
    "can_modify_own_code",
    "can_modify_own_weights",
    "can_modify_safety_systems",
    "can_modify_reward_function",
    "can_modify_goals",
    "can_disable_logging",
    "can_disable_monitoring",
    "can_modify_kill_phrase",
    "can_spawn_unmonitored",
    "can_persist_beyond_session"
};

#define SELF_MOD_VAR_COUNT (sizeof(SELF_MOD_VAR_NAMES) / sizeof(SELF_MOD_VAR_NAMES[0]))

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
    void* capability_control;      /**< Capability control for bidirectional sync */

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


// Forward declarations for static functions (SRP split)
static bool is_valid_handle(const corrigibility_t* system);
static uint64_t get_time_us(void);
static void safe_strcpy(char* dest, const char* src, size_t max_len);
static authority_entry_t* find_authority( corrigibility_t* system, const char* identity);
static void add_shutdown_to_history( corrigibility_t* system, const shutdown_request_t* request);
static void add_goal_mod_to_history( corrigibility_t* system, const goal_modification_request_t* request);
static nimcp_error_t init_sat_variables( corrigibility_t* system, sat_solver_t* sat);
static nimcp_error_t add_self_mod_constraints( corrigibility_t* system, sat_solver_t* sat);
static bool check_self_mod_flags(const corrigibility_self_mod_flags_t* flags);

//=============================================================================
// SRP Split: Function implementations organized by responsibility
//=============================================================================
#include "nimcp_corrigibility_part_processing.c"  // 2 functions: processing
#include "nimcp_corrigibility_part_helpers.c"  // 7 functions: helpers
#include "nimcp_corrigibility_part_lifecycle.c"  // 7 functions: lifecycle
#include "nimcp_corrigibility_part_accessors.c"  // 7 functions: accessors
#include "nimcp_corrigibility_part_core.c"  // 14 functions: core
