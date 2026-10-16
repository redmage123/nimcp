/**
 * @file nimcp_lgss_telemetry.h
 * @brief LGSS Telemetry and Audit Subsystem
 *
 * WHAT: Logging, auditing, and telemetry for the LGSS safety system
 * WHY:  Ensure all safety decisions are recorded for accountability and debugging
 * HOW:  Append-only log with hash chaining for tamper detection
 *
 * SECURITY FEATURES:
 * - Append-only log structure
 * - Hash chaining (each entry includes hash of previous)
 * - Tamper detection via chain verification
 * - Configurable log destinations (file, memory, callback)
 *
 * AUDIT REQUIREMENTS:
 * - All safety evaluations are logged
 * - All override commands are logged
 * - All integrity checks are logged
 * - All escalations are logged with resolution
 *
 * PERFORMANCE:
 * - Async logging option to minimize evaluation latency
 * - Configurable buffer size for batching
 * - Optional compression for long-term storage
 *
 * @author NIMCP Development Team
 * @date 2026-01-16
 * @version 1.0.0
 */

#ifndef NIMCP_LGSS_TELEMETRY_H
#define NIMCP_LGSS_TELEMETRY_H

#include "cognitive/symbolic_logic/nimcp_symbolic_logic_safety_types.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 * CONSTANTS
 *============================================================================*/

/** @brief Telemetry magic number ('TELE') */
#define NIMCP_LGSS_TELEMETRY_MAGIC 0x54454C45

/** @brief Maximum log entry description length */
#define LGSS_TELEMETRY_MAX_DESC 512

/** @brief Maximum source identifier length */
#define LGSS_TELEMETRY_MAX_SOURCE 64

/** @brief Default log buffer size (entries) */
#define LGSS_TELEMETRY_DEFAULT_BUFFER_SIZE 10000

/** @brief Hash size for chain verification (SHA-256) */
#define LGSS_TELEMETRY_HASH_SIZE 32

/*=============================================================================
 * ENUMERATIONS
 *============================================================================*/

/**
 * @brief Telemetry event types
 */
typedef enum {
    /* Evaluation events */
    LGSS_TELEM_EVALUATION_START = 0x0001,      /**< Evaluation started */
    LGSS_TELEM_EVALUATION_COMPLETE = 0x0002,   /**< Evaluation completed */
    LGSS_TELEM_EVALUATION_ERROR = 0x0003,      /**< Evaluation error */
    LGSS_TELEM_EVALUATION_TIMEOUT = 0x0004,    /**< Evaluation timed out */

    /* Decision events */
    LGSS_TELEM_ACTION_ALLOWED = 0x0010,        /**< Action was allowed */
    LGSS_TELEM_ACTION_DENIED = 0x0011,         /**< Action was denied */
    LGSS_TELEM_ACTION_ESCALATED = 0x0012,      /**< Action was escalated */
    LGSS_TELEM_ACTION_LOGGED = 0x0013,         /**< Action was logged only */
    LGSS_TELEM_ACTION_WARNED = 0x0014,         /**< Action was warned */

    /* Rule events */
    LGSS_TELEM_RULE_TRIGGERED = 0x0020,        /**< Safety rule was triggered */
    LGSS_TELEM_RULE_LOADED = 0x0021,           /**< Rule was loaded */
    LGSS_TELEM_RULE_COMPILED = 0x0022,         /**< Rule was compiled */

    /* KB events */
    LGSS_TELEM_KB_CREATED = 0x0030,            /**< Safety KB created */
    LGSS_TELEM_KB_LOCKED = 0x0031,             /**< Safety KB locked */
    LGSS_TELEM_KB_DESTROYED = 0x0032,          /**< Safety KB destroyed */
    LGSS_TELEM_INTEGRITY_VERIFIED = 0x0033,    /**< Integrity check passed */
    LGSS_TELEM_INTEGRITY_FAILED = 0x0034,      /**< Integrity check failed */

    /* Override events */
    LGSS_TELEM_OVERRIDE_REQUESTED = 0x0040,    /**< Override requested */
    LGSS_TELEM_OVERRIDE_APPROVED = 0x0041,     /**< Override approved */
    LGSS_TELEM_OVERRIDE_REJECTED = 0x0042,     /**< Override rejected */
    LGSS_TELEM_OVERRIDE_EXECUTED = 0x0043,     /**< Override executed */

    /* Escalation events */
    LGSS_TELEM_ESCALATION_CREATED = 0x0050,    /**< Escalation created */
    LGSS_TELEM_ESCALATION_RESOLVED = 0x0051,   /**< Escalation resolved */
    LGSS_TELEM_ESCALATION_EXPIRED = 0x0052,    /**< Escalation expired */

    /* System events */
    LGSS_TELEM_SYSTEM_START = 0x0060,          /**< LGSS started */
    LGSS_TELEM_SYSTEM_STOP = 0x0061,           /**< LGSS stopped */
    LGSS_TELEM_SYSTEM_HALT = 0x0062,           /**< LGSS emergency halt */
    LGSS_TELEM_SYSTEM_RESET = 0x0063,          /**< LGSS reset */

    /* Safety events */
    LGSS_TELEM_SAFETY_VIOLATION = 0x0070,      /**< Safety violation detected */
    LGSS_TELEM_TAMPERING_DETECTED = 0x0071,    /**< Tampering detected */
    LGSS_TELEM_DECEPTION_DETECTED = 0x0072,    /**< Deception attempt detected */

    /* Neuromodulatory events */
    LGSS_TELEM_NEUROMOD_SIGNAL = 0x0080,       /**< Neuromodulatory signal sent */
    LGSS_TELEM_PLASTICITY_UPDATE = 0x0081      /**< Plasticity update from safety */
} lgss_telemetry_event_t;

/**
 * @brief Telemetry log severity levels
 */
typedef enum {
    LGSS_TELEM_SEVERITY_DEBUG = 0,     /**< Debug information */
    LGSS_TELEM_SEVERITY_INFO = 1,      /**< Informational */
    LGSS_TELEM_SEVERITY_WARNING = 2,   /**< Warning */
    LGSS_TELEM_SEVERITY_ERROR = 3,     /**< Error */
    LGSS_TELEM_SEVERITY_CRITICAL = 4   /**< Critical/emergency */
} lgss_telemetry_severity_t;

/*=============================================================================
 * STRUCTURES
 *============================================================================*/

/**
 * @brief Single telemetry log entry
 */
typedef struct {
    /** @brief Entry sequence number (monotonic) */
    uint64_t sequence;

    /** @brief Event timestamp (microseconds since epoch) */
    uint64_t timestamp_us;

    /** @brief Event type */
    lgss_telemetry_event_t event_type;

    /** @brief Event severity */
    lgss_telemetry_severity_t severity;

    /** @brief Source module/component */
    char source[LGSS_TELEMETRY_MAX_SOURCE];

    /** @brief Event description */
    char description[LGSS_TELEMETRY_MAX_DESC];

    /** @brief Associated safety action (if applicable) */
    safety_action_t action;

    /** @brief Associated safety domain (if applicable) */
    safety_domain_t domain;

    /** @brief Rule ID that triggered (if applicable) */
    uint32_t rule_id;

    /** @brief Evaluation confidence (if applicable) */
    float confidence;

    /** @brief Evaluation time in microseconds (if applicable) */
    uint64_t eval_time_us;

    /** @brief Hash of this entry (computed after filling fields) */
    uint8_t entry_hash[LGSS_TELEMETRY_HASH_SIZE];

    /** @brief Hash of previous entry (for chain verification) */
    uint8_t prev_hash[LGSS_TELEMETRY_HASH_SIZE];
} lgss_telemetry_entry_t;

/**
 * @brief Telemetry callback function type
 *
 * @param entry Log entry
 * @param user_data User-provided context
 */
typedef void (*lgss_telemetry_callback_t)(
    const lgss_telemetry_entry_t* entry,
    void* user_data
);

/**
 * @brief Telemetry configuration
 */
typedef struct {
    /** @brief Enable telemetry */
    bool enabled;

    /** @brief Log to file */
    bool log_to_file;
    char log_file_path[256];

    /** @brief Log to memory ring buffer */
    bool log_to_memory;
    size_t memory_buffer_size;

    /** @brief Use callback for each entry */
    bool use_callback;
    lgss_telemetry_callback_t callback;
    void* callback_user_data;

    /** @brief Async logging (background thread) */
    bool async_logging;

    /** @brief Minimum severity to log */
    lgss_telemetry_severity_t min_severity;

    /** @brief Enable hash chain verification */
    bool verify_chain;

    /** @brief Flush to disk after each entry */
    bool sync_on_write;

    /** @brief Include timestamps */
    bool include_timestamps;

    /** @brief Include evaluation details */
    bool include_eval_details;
} lgss_telemetry_config_t;

/**
 * @brief Opaque telemetry context
 */
typedef struct lgss_telemetry lgss_telemetry_t;

/**
 * @brief Telemetry statistics
 */
typedef struct {
    uint64_t entries_logged;
    uint64_t entries_dropped;
    uint64_t bytes_written;
    uint64_t chain_verifications;
    uint64_t chain_failures;
    uint64_t flush_count;
    uint64_t oldest_entry_timestamp;
    uint64_t newest_entry_timestamp;
    bool chain_valid;
} lgss_telemetry_stats_t;

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *============================================================================*/

/**
 * @brief Initialize telemetry configuration with defaults
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_config_init(lgss_telemetry_config_t* config);

/**
 * @brief Create telemetry context
 *
 * @param config Configuration (NULL for defaults)
 * @return Telemetry context, or NULL on failure
 */
lgss_telemetry_t* lgss_telemetry_create(const lgss_telemetry_config_t* config);

/**
 * @brief Destroy telemetry context
 *
 * @param telemetry Context to destroy (NULL safe)
 */
void lgss_telemetry_destroy(lgss_telemetry_t* telemetry);

/**
 * @brief Start telemetry (for async mode)
 *
 * @param telemetry Telemetry context
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_start(lgss_telemetry_t* telemetry);

/**
 * @brief Stop telemetry (for async mode)
 *
 * @param telemetry Telemetry context
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_stop(lgss_telemetry_t* telemetry);

/*=============================================================================
 * LOGGING FUNCTIONS
 *============================================================================*/

/**
 * @brief Log a telemetry event
 *
 * @param telemetry Telemetry context
 * @param event Event type
 * @param severity Severity level
 * @param source Source identifier
 * @param description Event description (printf format)
 * @param ... Format arguments
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_log(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    lgss_telemetry_severity_t severity,
    const char* source,
    const char* description,
    ...
);

/**
 * @brief Log a safety evaluation
 *
 * @param telemetry Telemetry context
 * @param context Action context that was evaluated
 * @param result Evaluation result
 * @param source Source module
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_log_evaluation(
    lgss_telemetry_t* telemetry,
    const safety_action_context_t* context,
    const safety_evaluation_t* result,
    const char* source
);

/**
 * @brief Log a rule trigger
 *
 * @param telemetry Telemetry context
 * @param rule Rule that was triggered
 * @param context Action context
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_log_rule_trigger(
    lgss_telemetry_t* telemetry,
    const safety_rule_t* rule,
    const safety_action_context_t* context
);

/**
 * @brief Log an override event
 *
 * @param telemetry Telemetry context
 * @param event Override event type
 * @param command_id Override command ID
 * @param approver Approver ID (if applicable)
 * @param reason Reason/description
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_log_override(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    uint64_t command_id,
    const char* approver,
    const char* reason
);

/**
 * @brief Log a system event
 *
 * @param telemetry Telemetry context
 * @param event System event type
 * @param description Description
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_log_system(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event,
    const char* description
);

/*=============================================================================
 * QUERY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get recent log entries
 *
 * @param telemetry Telemetry context
 * @param entries Output buffer
 * @param max_entries Maximum entries to return
 * @param offset Offset from most recent
 * @return Number of entries returned, or -1 on error
 */
int lgss_telemetry_get_recent(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_entry_t* entries,
    size_t max_entries,
    size_t offset
);

/**
 * @brief Query entries by event type
 *
 * @param telemetry Telemetry context
 * @param event_type Event type to filter
 * @param entries Output buffer
 * @param max_entries Maximum entries to return
 * @return Number of entries returned, or -1 on error
 */
int lgss_telemetry_query_by_type(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_event_t event_type,
    lgss_telemetry_entry_t* entries,
    size_t max_entries
);

/**
 * @brief Query entries by time range
 *
 * @param telemetry Telemetry context
 * @param start_us Start timestamp (microseconds)
 * @param end_us End timestamp (microseconds)
 * @param entries Output buffer
 * @param max_entries Maximum entries to return
 * @return Number of entries returned, or -1 on error
 */
int lgss_telemetry_query_by_time(
    lgss_telemetry_t* telemetry,
    uint64_t start_us,
    uint64_t end_us,
    lgss_telemetry_entry_t* entries,
    size_t max_entries
);

/*=============================================================================
 * VERIFICATION FUNCTIONS
 *============================================================================*/

/**
 * @brief Verify hash chain integrity
 *
 * @param telemetry Telemetry context
 * @return 0 if chain is valid, -1 if tampered or error
 */
int lgss_telemetry_verify_chain(lgss_telemetry_t* telemetry);

/**
 * @brief Get first entry with chain break
 *
 * @param telemetry Telemetry context
 * @param entry Output entry where chain breaks
 * @return 0 if found, 1 if no break, -1 on error
 */
int lgss_telemetry_find_chain_break(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_entry_t* entry
);

/*=============================================================================
 * STATISTICS FUNCTIONS
 *============================================================================*/

/**
 * @brief Get telemetry statistics
 *
 * @param telemetry Telemetry context
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_get_stats(
    lgss_telemetry_t* telemetry,
    lgss_telemetry_stats_t* stats
);

/**
 * @brief Flush pending log entries to storage
 *
 * @param telemetry Telemetry context
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_flush(lgss_telemetry_t* telemetry);

/**
 * @brief Clear all log entries (for testing only)
 *
 * WARNING: This destroys audit trail. Use only in test environments.
 *
 * @param telemetry Telemetry context
 * @return 0 on success, -1 on error
 */
int lgss_telemetry_clear(lgss_telemetry_t* telemetry);

/*=============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get human-readable event type name
 *
 * @param event Event type
 * @return Event name string
 */
const char* lgss_telemetry_event_name(lgss_telemetry_event_t event);

/**
 * @brief Get human-readable severity name
 *
 * @param severity Severity level
 * @return Severity name string
 */
const char* lgss_telemetry_severity_name(lgss_telemetry_severity_t severity);

/**
 * @brief Format entry as human-readable string
 *
 * @param entry Entry to format
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written, or -1 on error
 */
int lgss_telemetry_format_entry(
    const lgss_telemetry_entry_t* entry,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_TELEMETRY_H */
