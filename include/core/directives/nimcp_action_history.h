/**
 * @file nimcp_action_history.h
 * @brief Action history tracking for combinatorial harm detection
 *
 * WHAT: Circular buffer-based action history tracker that maintains recent
 *       actions for combinatorial harm analysis
 * WHY:  Enables detection of harmful action sequences by tracking temporal
 *       patterns and allowing analysis of recent action combinations
 * HOW:  Thread-safe circular buffer with time-windowed queries, type filtering,
 *       and bio-async integration for cross-module coordination
 *
 * Biological basis: Models episodic memory in the hippocampus, which maintains
 * temporal sequences of events for pattern recognition and threat detection.
 * The time-windowed queries mirror the temporal decay of episodic memories.
 */

#ifndef NIMCP_ACTION_HISTORY_H
#define NIMCP_ACTION_HISTORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum history entries (circular buffer capacity) */
#define ACTION_HISTORY_MAX_ENTRIES 1024

/* Default time window for recent actions (1 minute) */
#define ACTION_HISTORY_DEFAULT_WINDOW_MS 60000

/* Action type string length */
#define ACTION_TYPE_MAX_LEN 64

/* Action description string length */
#define ACTION_DESC_MAX_LEN 256

/* Action data buffer size */
#define ACTION_DATA_MAX_LEN 512

/**
 * @brief Individual action record
 *
 * WHAT: Single action event with metadata and serialized details
 * WHY:  Captures all relevant information needed for combinatorial analysis
 * HOW:  Fixed-size structure optimized for circular buffer storage
 */
typedef struct {
    uint32_t action_id;             /* Unique action identifier */
    uint64_t timestamp_ms;          /* Millisecond timestamp */
    uint32_t source_module;         /* Module that generated action */
    char action_type[ACTION_TYPE_MAX_LEN];        /* Action type name */
    char action_description[ACTION_DESC_MAX_LEN]; /* Human-readable description */
    float predicted_harm_score;     /* Pre-execution harm score (0.0-1.0) */
    bool was_blocked;               /* Whether action was blocked */
    uint8_t action_data[ACTION_DATA_MAX_LEN];     /* Serialized action details */
    size_t action_data_len;         /* Actual data length */
} action_record_t;

/**
 * @brief Action history configuration
 *
 * WHAT: Configuration parameters for history tracker behavior
 * WHY:  Allows tuning of memory usage, time windows, and auto-pruning
 * HOW:  Simple struct with sensible defaults
 */
typedef struct {
    uint32_t max_history_size;      /* Max records (up to ACTION_HISTORY_MAX_ENTRIES) */
    uint64_t time_window_ms;        /* Default time window for queries */
    bool auto_prune;                /* Automatically prune old entries on record */
} action_history_config_t;

/**
 * @brief Action history statistics
 *
 * WHAT: Summary statistics about action history state
 * WHY:  Enables monitoring and diagnostics of history tracker
 * HOW:  Computed on-demand from current history state
 */
typedef struct {
    uint32_t total_records;         /* Total records currently stored */
    uint32_t blocked_count;         /* Number of blocked actions */
    uint32_t unique_types;          /* Number of unique action types */
    uint64_t oldest_timestamp_ms;   /* Timestamp of oldest record */
    uint64_t newest_timestamp_ms;   /* Timestamp of newest record */
    float avg_harm_score;           /* Average harm score across records */
    float max_harm_score;           /* Maximum harm score in history */
} action_history_stats_t;

/**
 * @brief Action history tracker (opaque)
 *
 * WHAT: Main action history management structure
 * WHY:  Encapsulates all state needed for thread-safe history tracking
 * HOW:  Circular buffer with mutex protection and bio-async integration
 */
typedef struct action_history_t action_history_t;

/**
 * @brief Get default configuration
 *
 * WHAT: Populates config with sensible defaults
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Sets max_history_size=1024, time_window_ms=60000, auto_prune=true
 *
 * @param config Configuration structure to populate
 */
void action_history_default_config(action_history_config_t* config);

/**
 * @brief Create action history tracker
 *
 * WHAT: Allocates and initializes action history tracker with circular buffer
 * WHY:  Required to begin tracking actions for combinatorial analysis
 * HOW:  Allocates circular buffer, initializes mutex, sets up bio-async context
 *
 * @param config Configuration parameters (NULL for defaults)
 * @return Initialized tracker or NULL on failure
 */
action_history_t* action_history_create(const action_history_config_t* config);

/**
 * @brief Destroy action history tracker
 *
 * WHAT: Frees all resources associated with history tracker
 * WHY:  Prevents memory leaks on shutdown
 * HOW:  Disconnects bio-async, destroys mutex, frees circular buffer and struct
 *
 * @param history History tracker to destroy
 */
void action_history_destroy(action_history_t* history);

/**
 * @brief Record action in history
 *
 * WHAT: Adds action record to circular buffer with thread safety
 * WHY:  Maintains temporal sequence of actions for analysis
 * HOW:  Acquires mutex, writes to circular buffer, auto-prunes if enabled
 *
 * Biological basis: Models synaptic consolidation in hippocampus, where
 * new episodic memories are encoded into the temporal sequence.
 *
 * @param history History tracker
 * @param record Action record to add (copied into circular buffer)
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_record(action_history_t* history, const action_record_t* record);

/**
 * @brief Get recent actions within time window
 *
 * WHAT: Retrieves actions recorded within specified time window
 * WHY:  Enables temporal analysis of recent action sequences
 * HOW:  Scans circular buffer for records within time window, copies to output
 *
 * Biological basis: Models working memory's temporal window, retrieving
 * recent episodic memories for pattern analysis.
 *
 * @param history History tracker
 * @param time_window_ms Time window in milliseconds (0 for all records)
 * @param out_records Output buffer for records
 * @param max_count Maximum records to retrieve
 * @param out_count Number of records actually retrieved
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_get_recent(action_history_t* history,
                               uint64_t time_window_ms,
                               action_record_t* out_records,
                               uint32_t max_count,
                               uint32_t* out_count);

/**
 * @brief Get actions by type
 *
 * WHAT: Retrieves all actions matching specific action type
 * WHY:  Enables type-specific analysis for combinatorial patterns
 * HOW:  Scans circular buffer for matching action_type, copies to output
 *
 * @param history History tracker
 * @param action_type Action type string to match
 * @param out_records Output buffer for records
 * @param max_count Maximum records to retrieve
 * @param out_count Number of records actually retrieved
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_get_by_type(action_history_t* history,
                                const char* action_type,
                                action_record_t* out_records,
                                uint32_t max_count,
                                uint32_t* out_count);

/**
 * @brief Get history statistics
 *
 * WHAT: Computes summary statistics from current history state
 * WHY:  Enables monitoring and diagnostics of action patterns
 * HOW:  Scans circular buffer to compute counts, averages, and extrema
 *
 * @param history History tracker
 * @param stats Output statistics structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_get_stats(action_history_t* history,
                              action_history_stats_t* stats);

/**
 * @brief Prune old entries from history
 *
 * WHAT: Removes action records older than specified timestamp
 * WHY:  Manages memory usage by removing stale data
 * HOW:  Scans circular buffer and marks old entries as invalid
 *
 * Biological basis: Models memory decay in hippocampus, where older
 * episodic memories fade over time to make room for new information.
 *
 * @param history History tracker
 * @param older_than_ms Remove records older than this timestamp
 * @return Number of records pruned, or negative NIMCP_ERROR_* on failure
 */
int action_history_prune(action_history_t* history, uint64_t older_than_ms);

/**
 * @brief Clear all history entries
 *
 * WHAT: Removes all action records from history
 * WHY:  Enables reset of history tracking (e.g., after major state change)
 * HOW:  Resets circular buffer head/tail pointers and count
 *
 * @param history History tracker
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_clear(action_history_t* history);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers action history as bio-async module for cross-module messaging
 * WHY:  Enables coordination with other directives modules via bio-async
 * HOW:  Registers with BIO_MODULE_CORE_DIRECTIVES_HISTORY, sets up inbox
 *
 * @param history History tracker
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_connect_bio_async(action_history_t* history);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters action history from bio-async router
 * WHY:  Cleanup before shutdown or to disable cross-module messaging
 * HOW:  Calls bio_router_unregister_module with stored context
 *
 * @param history History tracker
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int action_history_disconnect_bio_async(action_history_t* history);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Returns whether history tracker is connected to bio-async router
 * WHY:  Enables conditional bio-async operations
 * HOW:  Returns bio_async_enabled flag
 *
 * @param history History tracker
 * @return true if connected, false otherwise
 */
bool action_history_is_bio_async_connected(const action_history_t* history);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ACTION_HISTORY_H */
