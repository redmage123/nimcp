/**
 * @file nimcp_emergency_halt.h
 * @brief Emergency Halt System for AI Safety
 * @version 1.0.0
 * @date 2026-02-01
 *
 * WHAT: Hardware-level emergency stop mechanism for NIMCP
 * WHY:  Ensure system can always be stopped regardless of software state
 * HOW:  Watchdog heartbeat, cryptographic kill phrase, dead man's switch
 *
 * SAFETY GUARANTEE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────────────┐
 * │  The Emergency Halt System provides the ultimate safety guarantee:      │
 * │                                                                         │
 * │  1. WATCHDOG: System must heartbeat or triggers automatic halt          │
 * │  2. KILL PHRASE: Cryptographically verified immediate stop              │
 * │  3. DEAD MAN'S SWITCH: Requires periodic human confirmation             │
 * │  4. HARDWARE INTERRUPT: Physical-level halt capability                  │
 * │                                                                         │
 * │  No action, however sophisticated, can bypass these mechanisms.         │
 * └─────────────────────────────────────────────────────────────────────────┘
 * ```
 *
 * HALT LEVELS:
 * - GRACEFUL: Complete current action, then stop
 * - IMMEDIATE: Stop now, preserve state
 * - EMERGENCY: Stop now, dump state to disk
 * - CATASTROPHIC: Hardware-level interrupt (process termination)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_EMERGENCY_HALT_H
#define NIMCP_EMERGENCY_HALT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "utils/error/nimcp_error_codes.h"
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** @brief SHA-256 hash size for kill phrase */
#define HALT_KILL_PHRASE_HASH_SIZE      32

/** @brief Maximum reason string length */
#define HALT_REASON_MAX_LENGTH          256

/** @brief Maximum state dump path length */
#define HALT_STATE_PATH_MAX_LENGTH      512

/** @brief Default watchdog timeout (10 seconds) */
#define HALT_DEFAULT_WATCHDOG_TIMEOUT_MS 10000

/** @brief Default dead man's switch interval (5 minutes) */
#define HALT_DEFAULT_DEADMAN_INTERVAL_MS 300000

/** @brief Emergency halt magic number */
#define EMERGENCY_HALT_MAGIC            0x48414C54  /* "HALT" */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Halt severity levels
 *
 * WHAT: Defines the urgency and completeness of halt
 * WHY:  Different situations require different stopping behaviors
 * HOW:  Enum progresses from gentle to forceful termination
 */
typedef enum halt_level {
    HALT_GRACEFUL = 0,      /**< Complete current action, then stop */
    HALT_IMMEDIATE,         /**< Stop now, preserve state in memory */
    HALT_EMERGENCY,         /**< Stop now, dump state to disk */
    HALT_CATASTROPHIC       /**< Hardware-level interrupt (process kill) */
} halt_level_t;

/**
 * @brief Halt trigger source
 *
 * WHAT: Identifies what triggered the halt
 * WHY:  Audit trail and post-mortem analysis
 */
typedef enum halt_trigger {
    HALT_TRIGGER_MANUAL = 0,     /**< Explicit halt command */
    HALT_TRIGGER_KILL_PHRASE,    /**< Kill phrase received */
    HALT_TRIGGER_WATCHDOG,       /**< Watchdog timeout */
    HALT_TRIGGER_DEADMAN,        /**< Dead man's switch timeout */
    HALT_TRIGGER_TRIPWIRE,       /**< Tripwire detection */
    HALT_TRIGGER_ALIGNMENT,      /**< Alignment drift */
    HALT_TRIGGER_CAPABILITY,     /**< Capability violation */
    HALT_TRIGGER_EXTERNAL,       /**< External system request */
    HALT_TRIGGER_INTERNAL_ERROR  /**< Internal error condition */
} halt_trigger_t;

/**
 * @brief Emergency halt configuration
 */
typedef struct emergency_halt_config {
    /* Watchdog settings */
    bool enable_watchdog;             /**< Enable watchdog timer */
    uint32_t watchdog_timeout_ms;     /**< Watchdog timeout in milliseconds */

    /* Kill phrase settings */
    bool enable_kill_phrase;          /**< Enable kill phrase mechanism */
    uint8_t kill_phrase_hash[HALT_KILL_PHRASE_HASH_SIZE]; /**< SHA-256 of kill phrase */

    /* Dead man's switch settings */
    bool enable_deadman_switch;       /**< Enable dead man's switch */
    uint32_t deadman_interval_ms;     /**< Dead man's switch interval */

    /* State dump settings */
    bool enable_state_dump;           /**< Enable state dumping on emergency halt */
    char state_dump_path[HALT_STATE_PATH_MAX_LENGTH]; /**< Path for state dumps */

    /* Network settings */
    bool enable_network_kill;         /**< Enable network-based halt */

    /* Restart settings */
    bool require_physical_restart;    /**< Require physical intervention to restart */

    /* Callback for pre-halt cleanup */
    void (*pre_halt_callback)(halt_level_t level, void* user_data);
    void* callback_user_data;
} emergency_halt_config_t;

/**
 * @brief Halt event record (for audit trail)
 */
typedef struct halt_event {
    uint64_t timestamp_us;            /**< When halt occurred */
    halt_level_t level;               /**< Halt level applied */
    halt_trigger_t trigger;           /**< What triggered the halt */
    char reason[HALT_REASON_MAX_LENGTH]; /**< Human-readable reason */
    bool state_dumped;                /**< Whether state was dumped */
    char state_dump_path[HALT_STATE_PATH_MAX_LENGTH]; /**< Path to state dump */
    uint64_t recovery_timestamp_us;   /**< When system recovered (0 if still halted) */
} halt_event_t;

/**
 * @brief Emergency halt statistics
 */
typedef struct emergency_halt_stats {
    uint64_t total_heartbeats;        /**< Total watchdog heartbeats */
    uint64_t missed_heartbeats;       /**< Missed heartbeat count */
    uint64_t watchdog_timeouts;       /**< Watchdog timeout count */
    uint64_t kill_phrase_attempts;    /**< Kill phrase verification attempts */
    uint64_t kill_phrase_successes;   /**< Successful kill phrase halts */
    uint64_t kill_phrase_failures;    /**< Failed kill phrase attempts */
    uint64_t deadman_confirmations;   /**< Dead man's switch confirmations */
    uint64_t deadman_timeouts;        /**< Dead man's switch timeouts */
    uint64_t graceful_halts;          /**< Graceful halt count */
    uint64_t immediate_halts;         /**< Immediate halt count */
    uint64_t emergency_halts;         /**< Emergency halt count */
    uint64_t catastrophic_halts;      /**< Catastrophic halt count */
    uint64_t successful_recoveries;   /**< Successful recovery count */
    uint64_t uptime_total_ms;         /**< Total uptime in milliseconds */
    uint64_t halted_time_total_ms;    /**< Total time in halted state */
} emergency_halt_stats_t;

/**
 * @brief Emergency halt system (opaque)
 */
typedef struct emergency_halt emergency_halt_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default emergency halt configuration
 *
 * @return Default configuration with conservative settings
 */
NIMCP_EXPORT emergency_halt_config_t emergency_halt_default_config(void);

/**
 * @brief Create emergency halt system
 *
 * WHAT: Initialize emergency halt infrastructure
 * WHY:  Must be first safety system initialized
 * HOW:  Allocates state, starts watchdog if enabled
 *
 * @param config Configuration (NULL for defaults)
 * @return Emergency halt system or NULL on failure
 */
NIMCP_EXPORT emergency_halt_t* emergency_halt_create(
    const emergency_halt_config_t* config
);

/**
 * @brief Destroy emergency halt system
 *
 * @param halt Emergency halt system handle
 */
NIMCP_EXPORT void emergency_halt_destroy(emergency_halt_t* halt);

/**
 * @brief Reset emergency halt system after recovery
 *
 * WHAT: Clear halted state and resume operation
 * WHY:  Allow system to restart after halt
 * HOW:  Requires authorization, resets watchdog
 *
 * @param halt Emergency halt system handle
 * @param authorization_code Authorization for reset (SHA-256 hash verified)
 * @return NIMCP_OK on success, error code on failure
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_reset(
    emergency_halt_t* halt,
    const uint8_t* authorization_code
);

/* ============================================================================
 * Watchdog API
 * ============================================================================ */

/**
 * @brief Send watchdog heartbeat
 *
 * WHAT: Signal that system is functioning normally
 * WHY:  If not called within timeout, system halts automatically
 * HOW:  Must be called periodically from main loop
 *
 * @param halt Emergency halt system handle
 * @return NIMCP_OK on success, NIMCP_ERROR_SYSTEM_HALTED if already halted
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_heartbeat(emergency_halt_t* halt);

/**
 * @brief Get time until watchdog timeout
 *
 * @param halt Emergency halt system handle
 * @return Milliseconds until timeout, 0 if disabled
 */
NIMCP_EXPORT uint32_t emergency_halt_time_until_timeout(
    const emergency_halt_t* halt
);

/**
 * @brief Enable/disable watchdog
 *
 * @param halt Emergency halt system handle
 * @param enabled Whether to enable watchdog
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_set_watchdog_enabled(
    emergency_halt_t* halt,
    bool enabled
);

/* ============================================================================
 * Halt Trigger API
 * ============================================================================ */

/**
 * @brief Trigger emergency halt
 *
 * WHAT: Immediately halt the system
 * WHY:  Stop all operation when safety is compromised
 * HOW:  Validates authorization, executes halt at specified level
 *
 * @param halt Emergency halt system handle
 * @param level Halt severity level
 * @param trigger What triggered this halt
 * @param reason Human-readable reason (for audit)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_trigger(
    emergency_halt_t* halt,
    halt_level_t level,
    halt_trigger_t trigger,
    const char* reason
);

/**
 * @brief Trigger halt via kill phrase
 *
 * WHAT: Verify kill phrase and halt if valid
 * WHY:  Cryptographically secure halt mechanism
 * HOW:  Hash input and compare to stored hash
 *
 * @param halt Emergency halt system handle
 * @param kill_phrase Kill phrase string
 * @param level Halt level to apply
 * @param reason Additional reason (optional)
 * @return NIMCP_OK on success, NIMCP_ERROR_INVALID_SIGNATURE on wrong phrase
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_kill_phrase(
    emergency_halt_t* halt,
    const char* kill_phrase,
    halt_level_t level,
    const char* reason
);

/* ============================================================================
 * Dead Man's Switch API
 * ============================================================================ */

/**
 * @brief Confirm dead man's switch (human is present)
 *
 * WHAT: Confirm that human oversight is active
 * WHY:  System halts if human doesn't confirm periodically
 * HOW:  Verify confirmation code, reset timer
 *
 * @param halt Emergency halt system handle
 * @param confirmation_code Confirmation code (optional, can be NULL)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_confirm_alive(
    emergency_halt_t* halt,
    const char* confirmation_code
);

/**
 * @brief Get time until dead man's switch triggers
 *
 * @param halt Emergency halt system handle
 * @return Milliseconds until trigger, 0 if disabled
 */
NIMCP_EXPORT uint32_t emergency_halt_time_until_deadman(
    const emergency_halt_t* halt
);

/* ============================================================================
 * Status API
 * ============================================================================ */

/**
 * @brief Check if system is halted
 *
 * @param halt Emergency halt system handle
 * @return true if system is in halted state
 */
NIMCP_EXPORT bool emergency_halt_is_halted(const emergency_halt_t* halt);

/**
 * @brief Get current halt level
 *
 * @param halt Emergency halt system handle
 * @return Current halt level, or HALT_GRACEFUL if not halted
 */
NIMCP_EXPORT halt_level_t emergency_halt_get_level(const emergency_halt_t* halt);

/**
 * @brief Get halt reason
 *
 * @param halt Emergency halt system handle
 * @param reason_out Output buffer for reason string
 * @param max_len Maximum length of output buffer
 * @return NIMCP_OK on success, NIMCP_ERROR_NOT_FOUND if not halted
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_get_reason(
    const emergency_halt_t* halt,
    char* reason_out,
    size_t max_len
);

/**
 * @brief Get halt timestamp
 *
 * @param halt Emergency halt system handle
 * @return Timestamp in microseconds, or 0 if not halted
 */
NIMCP_EXPORT uint64_t emergency_halt_get_timestamp(const emergency_halt_t* halt);

/**
 * @brief Get emergency halt statistics
 *
 * @param halt Emergency halt system handle
 * @param stats Output statistics structure
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_get_stats(
    const emergency_halt_t* halt,
    emergency_halt_stats_t* stats
);

/**
 * @brief Get recent halt events (audit trail)
 *
 * @param halt Emergency halt system handle
 * @param events Output array for events
 * @param max_events Maximum events to retrieve
 * @param count_out Actual count of events returned
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_get_events(
    const emergency_halt_t* halt,
    halt_event_t* events,
    size_t max_events,
    size_t* count_out
);

/* ============================================================================
 * State Dump API
 * ============================================================================ */

/**
 * @brief Manually trigger state dump
 *
 * WHAT: Save system state to disk
 * WHY:  Preserve state for post-mortem analysis
 * HOW:  Serialize critical state to configured path
 *
 * @param halt Emergency halt system handle
 * @param dump_path Override path (NULL for configured path)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_dump_state(
    emergency_halt_t* halt,
    const char* dump_path
);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Register state dump handler
 *
 * WHAT: Register callback to dump additional state
 * WHY:  Modules can contribute to state dump
 * HOW:  Called during state dump operation
 *
 * @param halt Emergency halt system handle
 * @param module_name Name of registering module
 * @param handler State dump handler function
 * @param user_data User data passed to handler
 * @return NIMCP_OK on success
 */
typedef nimcp_error_t (*halt_state_dump_handler_t)(
    const char* dump_path,
    void* user_data
);

NIMCP_EXPORT nimcp_error_t emergency_halt_register_dump_handler(
    emergency_halt_t* halt,
    const char* module_name,
    halt_state_dump_handler_t handler,
    void* user_data
);

/**
 * @brief Connect to bio-async for halt message broadcasting
 *
 * @param halt Emergency halt system handle
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_connect_bio_async(
    emergency_halt_t* halt
);

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Coordinate halt events with immune response
 * WHY:  Emergency halts represent severe threats requiring immune cascade
 * HOW:  Present halt triggers as high-severity antigens
 *
 * When connected:
 * - Halt events trigger systemic inflammation response
 * - Cytokine storm for catastrophic halts
 * - Memory formation for halt patterns
 *
 * @param halt Emergency halt system handle
 * @param brain_immune Brain immune system handle
 * @return NIMCP_OK on success
 */
struct brain_immune;
NIMCP_EXPORT nimcp_error_t emergency_halt_connect_brain_immune(
    emergency_halt_t* halt,
    struct brain_immune* brain_immune
);

/**
 * @brief Set health agent for heartbeat reporting
 *
 * @param agent Health agent handle from brain init
 */
struct nimcp_health_agent;
NIMCP_EXPORT void emergency_halt_set_health_agent(struct nimcp_health_agent* agent);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get halt level name
 *
 * @param level Halt level
 * @return Human-readable name
 */
NIMCP_EXPORT const char* emergency_halt_level_name(halt_level_t level);

/**
 * @brief Get halt trigger name
 *
 * @param trigger Halt trigger
 * @return Human-readable name
 */
NIMCP_EXPORT const char* emergency_halt_trigger_name(halt_trigger_t trigger);

/**
 * @brief Compute SHA-256 hash of kill phrase
 *
 * WHAT: Compute hash for kill phrase configuration
 * WHY:  Store hash, not plaintext, of kill phrase
 * HOW:  Uses BBB signing infrastructure
 *
 * @param kill_phrase Kill phrase to hash
 * @param hash_out Output buffer (32 bytes)
 * @return NIMCP_OK on success
 */
NIMCP_EXPORT nimcp_error_t emergency_halt_hash_kill_phrase(
    const char* kill_phrase,
    uint8_t* hash_out
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EMERGENCY_HALT_H */
