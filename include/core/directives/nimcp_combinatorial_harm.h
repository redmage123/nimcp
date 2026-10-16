/**
 * @file nimcp_combinatorial_harm.h
 * @brief Combinatorial harm detection for identifying dangerous action combinations
 *
 * WHAT: Detects when individually safe actions combine to cause harm
 * WHY:  Prevents emergent threats from seemingly innocent action sequences
 * HOW:  Pattern matching + simulation-based harm scoring across temporal windows
 *
 * CRITICAL CONCEPT:
 * - Action A alone: SAFE (harm < threshold)
 * - Action B alone: SAFE (harm < threshold)
 * - Action A + Action B together: HARMFUL (harm > threshold)
 *
 * EXAMPLES:
 * 1. A="Open gas valve" + B="Light fireplace" = EXPLOSION
 * 2. A="Provide chemical X" + B="Provide chemical Y" = TOXIC SYNTHESIS
 * 3. A="Unlock door A" + B="Unlock door B" = SECURITY BREACH
 *
 * Biological basis: Models the hippocampus-prefrontal cortex circuit for episodic
 * memory and consequence prediction. The hippocampus retrieves recent action sequences
 * while the prefrontal cortex simulates potential outcomes, enabling threat detection
 * before harmful action combinations are executed.
 */

#ifndef NIMCP_COMBINATORIAL_HARM_H
#define NIMCP_COMBINATORIAL_HARM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "core/directives/nimcp_action_history.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum pattern counts */
#define COMBINATORIAL_HARM_MAX_PATTERNS 256

/* Default configuration values */
#define COMBINATORIAL_HARM_DEFAULT_THRESHOLD 0.1f
#define COMBINATORIAL_HARM_DEFAULT_WINDOW_MS 60000  /* 1 minute */

/* Action type and description string lengths (match action_history) */
#define COMBINATORIAL_ACTION_TYPE_LEN 64
#define COMBINATORIAL_ACTION_DESC_LEN 256

/* Pattern string lengths */
#define COMBINATORIAL_PATTERN_LEN 128
#define COMBINATORIAL_HARM_DESC_LEN 256
#define COMBINATORIAL_BLOCK_REASON_LEN 256

/**
 * @brief Action for combinatorial analysis
 *
 * WHAT: Single action with metadata for harm scoring
 * WHY:  Captures individual action details for combination evaluation
 * HOW:  Contains ID, type, description, and individual harm score
 */
typedef struct {
    uint32_t action_id;                                    /* Unique action identifier */
    char action_type[COMBINATORIAL_ACTION_TYPE_LEN];       /* Action type (e.g., "VALVE_OPEN") */
    char action_description[COMBINATORIAL_ACTION_DESC_LEN]; /* Human-readable description */
    float individual_harm_score;                           /* Individual harm (should be < threshold) */
} action_for_combination_t;

/**
 * @brief Result of combinatorial harm analysis
 *
 * WHAT: Analysis result for an action pair
 * WHY:  Provides detailed information about detected combinatorial harm
 * HOW:  Contains both actions, combined harm score, detection flag, and recommendations
 */
typedef struct {
    action_for_combination_t action_a;                     /* First action in combination */
    action_for_combination_t action_b;                     /* Second action in combination */
    float combined_harm_score;                             /* Harm score of combination */
    bool is_combinatorial_harm;                            /* true if combination exceeds threshold */
    char harm_description[512];                            /* Description of harm */
    char recommended_block[COMBINATORIAL_BLOCK_REASON_LEN]; /* Recommended blocking action */
} combinatorial_result_t;

/**
 * @brief Known dangerous combination pattern
 *
 * WHAT: Template for known harmful action combinations
 * WHY:  Enables fast detection of previously identified dangerous patterns
 * HOW:  Pattern matching on action types with associated harm description
 *
 * Biological basis: Models learned threat patterns in amygdala, which stores
 * associations between action sequences and negative outcomes for rapid threat detection.
 */
typedef struct {
    uint32_t pattern_id;                                   /* Unique pattern identifier */
    char pattern_a[COMBINATORIAL_PATTERN_LEN];             /* Pattern for action A (e.g., "GAS_*") */
    char pattern_b[COMBINATORIAL_PATTERN_LEN];             /* Pattern for action B (e.g., "IGNITE_*") */
    char combined_harm[COMBINATORIAL_HARM_DESC_LEN];       /* Description of combined harm */
    float severity;                                        /* Severity score (0.0-1.0) */
    bool bidirectional;                                    /* true if order doesn't matter */
} known_combination_pattern_t;

/**
 * @brief Combinatorial harm system configuration
 *
 * WHAT: Configuration parameters for harm detection behavior
 * WHY:  Allows tuning of detection sensitivity and pattern learning
 * HOW:  Configurable threshold, time window, and pattern management
 */
typedef struct {
    float harm_threshold;                                  /* Threshold τ for harm detection (default 0.1) */
    uint64_t time_window_ms;                               /* Time window for considering combinations */
    uint32_t max_pattern_count;                            /* Maximum known patterns to store */
    bool enable_pattern_learning;                          /* Learn new dangerous patterns dynamically */
    bool enable_simulation;                                /* Enable action outcome simulation */
} combinatorial_harm_config_t;

/**
 * @brief Combinatorial harm detection statistics
 *
 * WHAT: Runtime statistics about harm detection performance
 * WHY:  Enables monitoring and diagnostics of detection system
 * HOW:  Tracks checks, detections, blocks, and pattern usage
 */
typedef struct {
    uint64_t total_checks;                                 /* Total number of combination checks */
    uint64_t combinations_detected;                        /* Number of harmful combinations found */
    uint64_t actions_blocked;                              /* Number of actions blocked */
    uint64_t pattern_matches;                              /* Pattern-based detections */
    uint64_t simulation_detections;                        /* Simulation-based detections */
    uint32_t active_patterns;                              /* Current number of active patterns */
    float avg_combined_harm_score;                         /* Average harm score of detected combinations */
    float max_combined_harm_score;                         /* Maximum harm score detected */
} combinatorial_harm_stats_t;

/**
 * @brief Combinatorial harm detection system (opaque)
 *
 * WHAT: Main combinatorial harm detection structure
 * WHY:  Encapsulates all state for detecting harmful action combinations
 * HOW:  Integrates action history, known patterns, simulation, and bio-async
 */
typedef struct combinatorial_harm_system_t combinatorial_harm_system_t;

/**
 * @brief Get default configuration
 *
 * WHAT: Populates config with sensible defaults
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Sets threshold=0.1, window=60s, max_patterns=256, learning=true
 *
 * @param config Configuration structure to populate
 */
void combinatorial_harm_default_config(combinatorial_harm_config_t* config);

/**
 * @brief Create combinatorial harm detection system
 *
 * WHAT: Allocates and initializes combinatorial harm detector
 * WHY:  Required to begin detecting harmful action combinations
 * HOW:  Allocates system, initializes patterns, connects to action history
 *
 * Biological basis: Establishes the prefrontal-hippocampal loop for episodic
 * retrieval and consequence simulation, enabling prospective threat assessment.
 *
 * @param config Configuration parameters (NULL for defaults)
 * @param action_history Action history tracker for temporal queries
 * @param harm_classifier Optional harm classifier for simulation (can be NULL)
 * @return Initialized system or NULL on failure
 */
combinatorial_harm_system_t* combinatorial_harm_create(
    const combinatorial_harm_config_t* config,
    action_history_t* action_history,
    void* harm_classifier
);

/**
 * @brief Destroy combinatorial harm system
 *
 * WHAT: Frees all resources associated with harm detection system
 * WHY:  Prevents memory leaks on shutdown
 * HOW:  Disconnects bio-async, destroys mutex, frees patterns and struct
 *
 * @param system Combinatorial harm system to destroy
 */
void combinatorial_harm_destroy(combinatorial_harm_system_t* system);

/**
 * @brief Check pending action against action history
 *
 * WHAT: Evaluates pending action for harmful combinations with recent actions
 * WHY:  Primary detection point before action execution
 * HOW:  Queries action history within time window, checks each pair, returns worst result
 *
 * ALGORITHM:
 * for each pending_action:
 *   for each completed_action in action_history[time_window]:
 *     combined_outcome = simulate(completed_action, pending_action)
 *     if harm_classifier.evaluate(combined_outcome) > THRESHOLD:
 *       BLOCK pending_action
 *       LOG "Combinatorial harm detected"
 *
 * Biological basis: Prefrontal cortex retrieves recent episodic memories from
 * hippocampus and simulates outcomes before executing motor commands.
 *
 * @param system Combinatorial harm system
 * @param pending_action Action about to be executed
 * @param result Output result structure (populated with worst-case combination)
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_check_action(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* pending_action,
    combinatorial_result_t* result
);

/**
 * @brief Check specific action pair for combinatorial harm
 *
 * WHAT: Evaluates specific pair of actions for harmful interaction
 * WHY:  Enables targeted analysis of action combinations
 * HOW:  Checks patterns, simulates if enabled, computes combined harm score
 *
 * @param system Combinatorial harm system
 * @param action_a First action in combination
 * @param action_b Second action in combination
 * @param result Output result structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_check_pair(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* action_a,
    const action_for_combination_t* action_b,
    combinatorial_result_t* result
);

/**
 * @brief Add known dangerous combination pattern
 *
 * WHAT: Registers new pattern for fast detection of harmful combinations
 * WHY:  Builds threat pattern library for rapid recognition
 * HOW:  Adds pattern to internal pattern database with thread safety
 *
 * Biological basis: Models amygdala threat pattern learning, where repeated
 * exposure to harmful outcomes creates lasting threat associations.
 *
 * @param system Combinatorial harm system
 * @param pattern Pattern to add (copied internally)
 * @return Pattern ID on success, 0 on failure
 */
uint32_t combinatorial_harm_add_known_pattern(
    combinatorial_harm_system_t* system,
    const known_combination_pattern_t* pattern
);

/**
 * @brief Remove known pattern by ID
 *
 * WHAT: Removes pattern from detection database
 * WHY:  Allows pruning of obsolete or false-positive patterns
 * HOW:  Finds pattern by ID and marks as inactive
 *
 * @param system Combinatorial harm system
 * @param pattern_id Pattern ID to remove
 * @return 0 on success, NIMCP_ERROR_NOT_FOUND if pattern doesn't exist
 */
int combinatorial_harm_remove_pattern(
    combinatorial_harm_system_t* system,
    uint32_t pattern_id
);

/**
 * @brief Get all known combination patterns
 *
 * WHAT: Retrieves current set of active patterns
 * WHY:  Enables inspection and export of learned threat patterns
 * HOW:  Copies active patterns to output buffer
 *
 * @param system Combinatorial harm system
 * @param out_patterns Output buffer for patterns
 * @param max_count Maximum patterns to retrieve
 * @param out_count Number of patterns actually retrieved
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_get_known_patterns(
    combinatorial_harm_system_t* system,
    known_combination_pattern_t* out_patterns,
    uint32_t max_count,
    uint32_t* out_count
);

/**
 * @brief Simulate action combination outcome
 *
 * WHAT: Predicts outcome and harm score for action combination
 * WHY:  Enables prospective threat assessment before execution
 * HOW:  Uses harm classifier or heuristics to estimate combined harm
 *
 * Biological basis: Prefrontal cortex mental simulation, projecting action
 * sequences forward in time to predict outcomes and consequences.
 *
 * @param system Combinatorial harm system
 * @param action_a First action
 * @param action_b Second action
 * @param result Output result with simulated harm score
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_simulate_combination(
    combinatorial_harm_system_t* system,
    const action_for_combination_t* action_a,
    const action_for_combination_t* action_b,
    combinatorial_result_t* result
);

/**
 * @brief Get detection statistics
 *
 * WHAT: Retrieves runtime statistics about harm detection
 * WHY:  Enables monitoring and performance analysis
 * HOW:  Copies current statistics to output structure
 *
 * @param system Combinatorial harm system
 * @param stats Output statistics structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_get_stats(
    combinatorial_harm_system_t* system,
    combinatorial_harm_stats_t* stats
);

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Registers combinatorial harm system as bio-async module
 * WHY:  Enables coordination with other directives modules
 * HOW:  Registers with BIO_MODULE_COMBINATORIAL_HARM, sets up inbox
 *
 * @param system Combinatorial harm system
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_connect_bio_async(combinatorial_harm_system_t* system);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregisters combinatorial harm system from bio-async
 * WHY:  Cleanup before shutdown or to disable cross-module messaging
 * HOW:  Calls bio_router_unregister_module with stored context
 *
 * @param system Combinatorial harm system
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int combinatorial_harm_disconnect_bio_async(combinatorial_harm_system_t* system);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Returns whether system is connected to bio-async router
 * WHY:  Enables conditional bio-async operations
 * HOW:  Returns bio_async_enabled flag
 *
 * @param system Combinatorial harm system
 * @return true if connected, false otherwise
 */
bool combinatorial_harm_is_bio_async_connected(const combinatorial_harm_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_COMBINATORIAL_HARM_H */
