/**
 * @file nimcp_cingulate_adapter.h
 * @brief Brain adapter for Cingulate Cortex integration
 *
 * WHAT: Unified adapter connecting Cingulate Cortex sub-modules to the brain system
 * WHY:  Enable conflict monitoring, error detection, and emotion-cognition integration
 * HOW:  Orchestrates anterior and posterior cingulate processors as a cohesive unit
 *
 * ARCHITECTURE:
 * - Wraps both cingulate sub-modules (anterior ACC, posterior PCC)
 * - Provides high-level API for conflict monitoring and error processing
 * - Integrates with executive control for cognitive adjustment
 * - Connects to event bus for inter-module communication
 * - Supports training through backpropagation adapters
 *
 * BIOLOGICAL BASIS:
 * - Anterior Cingulate Cortex (ACC): Brodmann areas 24, 32, 33
 *   - Dorsal ACC (dACC): Conflict monitoring, error detection, cognitive control
 *   - Rostral ACC (rACC): Emotional processing, pain perception
 * - Posterior Cingulate Cortex (PCC): Brodmann areas 23, 31
 *   - Self-referential processing, autobiographical memory access
 *   - Default Mode Network hub, mind wandering
 * - Midcingulate Cortex (MCC): Brodmann area 24'
 *   - Motor planning, action selection
 *
 * ERROR-RELATED NEGATIVITY (ERN):
 * The dACC generates ERN ~50-100ms after errors, reflecting:
 * - Mismatch between expected and actual outcomes
 * - Conflict between competing response tendencies
 * - Need for behavioral adjustment
 *
 * @version Phase B4: Cingulate Cortex Integration
 * @date 2025-12-30
 */

#ifndef NIMCP_CINGULATE_ADAPTER_H
#define NIMCP_CINGULATE_ADAPTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Bio-async communication system */
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Logging system */
#include "utils/logging/nimcp_logging.h"

/* Unified memory system */
#include "utils/memory/nimcp_unified_memory.h"

/* Forward declaration for opaque adapter type */
typedef struct cingulate_adapter cingulate_adapter_t;

/*=============================================================================
 * CONFIGURATION
 *===========================================================================*/

/**
 * @brief Default configuration values
 */
#define CINGULATE_DEFAULT_MAX_CONFLICTS        32
#define CINGULATE_DEFAULT_MAX_ERRORS           64
#define CINGULATE_DEFAULT_CONFLICT_THRESHOLD   0.5f
#define CINGULATE_DEFAULT_ERROR_THRESHOLD      0.3f
#define CINGULATE_DEFAULT_ERN_WINDOW_MS        100.0f
#define CINGULATE_DEFAULT_ADAPTATION_RATE      0.1f

/**
 * @brief Cingulate cortex adapter configuration
 */
typedef struct {
    /* Capacity limits */
    uint32_t max_conflicts;              /**< Maximum tracked conflicts */
    uint32_t max_errors;                 /**< Maximum tracked errors */
    uint32_t response_options;           /**< Number of response alternatives */

    /* Threshold settings */
    float conflict_threshold;            /**< Threshold for conflict detection [0, 1] */
    float error_threshold;               /**< Threshold for error detection [0, 1] */
    float adjustment_threshold;          /**< Threshold for triggering adjustment */

    /* Timing parameters */
    float ern_window_ms;                 /**< Error-related negativity window */
    float adaptation_decay_ms;           /**< Adaptation signal decay time */

    /* ACC settings */
    bool enable_conflict_monitoring;     /**< Enable conflict detection */
    bool enable_error_detection;         /**< Enable error-related processing */
    bool enable_cognitive_control;       /**< Enable top-down control signals */

    /* PCC settings */
    bool enable_self_referential;        /**< Enable self-referential processing */
    bool enable_dmn_integration;         /**< Enable Default Mode Network integration */
    bool enable_autobio_access;          /**< Enable autobiographical memory access */

    /* Learning */
    bool enable_learning;                /**< Enable adaptive learning */
    float adaptation_rate;               /**< Rate of behavioral adaptation */

    /* Event system */
    bool enable_events;                  /**< Enable event bus integration */

    /* Bio-async communication */
    bool enable_bio_async;               /**< Enable bio-async messaging */
    nimcp_bio_channel_type_t default_channel; /**< Default neuromodulator channel */
} cingulate_config_t;

/*=============================================================================
 * STATUS AND STATE
 *===========================================================================*/

/**
 * @brief Processing status of the adapter
 */
typedef enum {
    CINGULATE_STATUS_IDLE = 0,           /**< Ready for input */
    CINGULATE_STATUS_MONITORING,         /**< Monitoring for conflicts */
    CINGULATE_STATUS_CONFLICT_DETECTED,  /**< Conflict detected, evaluating */
    CINGULATE_STATUS_ERROR_DETECTED,     /**< Error detected, generating ERN */
    CINGULATE_STATUS_ADJUSTING,          /**< Generating adjustment signal */
    CINGULATE_STATUS_SELF_REFERENTIAL,   /**< PCC self-referential processing */
    CINGULATE_STATUS_READY,              /**< Output ready for retrieval */
    CINGULATE_STATUS_ERROR               /**< Error state */
} cingulate_status_t;

/**
 * @brief Error codes for cingulate operations
 */
typedef enum {
    CINGULATE_ERROR_NONE = 0,
    CINGULATE_ERROR_INVALID_INPUT,
    CINGULATE_ERROR_CONFLICT_OVERFLOW,
    CINGULATE_ERROR_ERROR_OVERFLOW,
    CINGULATE_ERROR_THRESHOLD_INVALID,
    CINGULATE_ERROR_BUFFER_OVERFLOW,
    CINGULATE_ERROR_NOT_INITIALIZED,
    CINGULATE_ERROR_INTERNAL
} cingulate_error_t;

/*=============================================================================
 * CONFLICT AND ERROR STRUCTURES
 *===========================================================================*/

/**
 * @brief Response option for conflict monitoring
 */
typedef struct {
    uint32_t option_id;                  /**< Unique option identifier */
    float activation;                    /**< Current activation level [0, 1] */
    float evidence;                      /**< Accumulated evidence [0, 1] */
    float prior_probability;             /**< Prior probability of selection */
    uint8_t category;                    /**< Response category */
    bool is_prepotent;                   /**< Is this the dominant response? */
} cingulate_response_option_t;

/**
 * @brief Conflict event detected by ACC
 */
typedef struct {
    uint32_t conflict_id;                /**< Unique conflict identifier */
    uint32_t option_a_id;                /**< First conflicting option */
    uint32_t option_b_id;                /**< Second conflicting option */
    float conflict_level;                /**< Conflict intensity [0, 1] */
    float energy;                        /**< Conflict energy (N2 amplitude analog) */
    double timestamp_ms;                 /**< When conflict detected */
    bool requires_control;               /**< Needs executive intervention */
} cingulate_conflict_t;

/**
 * @brief Error event detected by ACC (generates ERN)
 */
typedef struct {
    uint32_t error_id;                   /**< Unique error identifier */
    uint32_t executed_option;            /**< What was actually done */
    uint32_t intended_option;            /**< What should have been done */
    float error_magnitude;               /**< Error severity [0, 1] */
    float ern_amplitude;                 /**< Error-related negativity amplitude */
    float pe_amplitude;                  /**< Positivity error (Pe) amplitude */
    double timestamp_ms;                 /**< When error detected */
    bool is_conscious;                   /**< Error reached conscious awareness */
} cingulate_error_event_t;

/**
 * @brief Cognitive control adjustment signal
 */
typedef struct {
    float control_level;                 /**< Current control level [0, 1] */
    float adjustment_magnitude;          /**< How much to adjust */
    float threshold_shift;               /**< Shift in response threshold */
    float attention_boost;               /**< Attention enhancement signal */
    uint32_t target_module;              /**< Target for control signal */
    bool apply_inhibition;               /**< Inhibit prepotent response */
    bool apply_enhancement;              /**< Enhance controlled response */
} cingulate_control_signal_t;

/**
 * @brief Self-referential processing result (PCC)
 */
typedef struct {
    float self_relevance;                /**< How self-relevant [0, 1] */
    float autobio_match;                 /**< Match to autobiographical memory */
    float mentalizing_level;             /**< Theory of Mind engagement */
    float internal_focus;                /**< Internal vs external attention */
    bool is_default_mode;                /**< DMN active state */
    uint32_t associated_memory_id;       /**< Related autobiographical memory */
} cingulate_self_reference_t;

/*=============================================================================
 * STATISTICS
 *===========================================================================*/

/**
 * @brief Adapter statistics
 */
typedef struct {
    /* Processing counts */
    uint64_t responses_monitored;        /**< Total response sets monitored */
    uint64_t conflicts_detected;         /**< Total conflicts detected */
    uint64_t errors_detected;            /**< Total errors detected */
    uint64_t adjustments_made;           /**< Total control adjustments */
    uint64_t self_references;            /**< Total self-referential events */

    /* Conflict metrics */
    float avg_conflict_level;            /**< Average conflict intensity */
    float max_conflict_level;            /**< Maximum conflict observed */
    float conflict_resolution_rate;      /**< Rate of successful resolution */

    /* Error metrics */
    float avg_error_magnitude;           /**< Average error severity */
    float avg_ern_amplitude;             /**< Average ERN amplitude */
    float error_awareness_rate;          /**< Rate of conscious error detection */

    /* Timing */
    float avg_detection_latency_ms;      /**< Average detection latency */
    float avg_adjustment_latency_ms;     /**< Average adjustment latency */

    /* Learning */
    uint64_t adaptations_applied;        /**< Successful adaptations */
    float current_control_level;         /**< Current cognitive control level */
} cingulate_stats_t;

/*=============================================================================
 * CALLBACK TYPES
 *===========================================================================*/

/**
 * @brief Callback for conflict notification
 */
typedef void (*cingulate_conflict_callback_t)(
    const cingulate_conflict_t* conflict,
    void* user_data
);

/**
 * @brief Callback for error notification
 */
typedef void (*cingulate_error_callback_t)(
    const cingulate_error_event_t* error,
    void* user_data
);

/**
 * @brief Callback for control signal generation
 */
typedef void (*cingulate_control_callback_t)(
    const cingulate_control_signal_t* signal,
    void* user_data
);

/**
 * @brief Callback for event notification
 */
typedef void (*cingulate_event_callback_t)(
    uint32_t event_type,
    const void* event_data,
    void* user_data
);

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get default configuration
 *
 * WHAT: Returns default configuration for cingulate adapter
 * WHY:  Provide sensible defaults for common use cases
 * HOW:  Initialize all fields with biologically-motivated values
 *
 * @return Default configuration structure
 */
cingulate_config_t cingulate_default_config(void);

/**
 * @brief Create cingulate cortex adapter
 *
 * WHAT: Allocate and initialize the adapter with ACC and PCC modules
 * WHY:  Central point for conflict monitoring and error detection
 * HOW:  Create ACC and PCC processors; initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New adapter instance, or NULL on failure
 */
cingulate_adapter_t* cingulate_create(const cingulate_config_t* config);

/**
 * @brief Destroy cingulate cortex adapter
 *
 * WHAT: Free all resources associated with the adapter
 * WHY:  Prevent memory leaks
 * HOW:  Destroy sub-modules, free buffers
 *
 * @param adapter Adapter to destroy
 */
void cingulate_destroy(cingulate_adapter_t* adapter);

/**
 * @brief Reset adapter state
 *
 * WHAT: Clear buffers and reset to idle state
 * WHY:  Prepare for new monitoring session
 * HOW:  Reset all sub-modules, clear conflict/error history
 *
 * @param adapter Adapter instance
 * @return true on success, false on failure
 */
bool cingulate_reset(cingulate_adapter_t* adapter);

/*=============================================================================
 * CONFLICT MONITORING (ACC - Dorsal)
 *===========================================================================*/

/**
 * @brief Begin monitoring response competition
 *
 * WHAT: Start monitoring a set of response alternatives
 * WHY:  Detect conflicts between competing responses
 * HOW:  Initialize monitoring state with response options
 *
 * @param adapter Adapter instance
 * @param num_options Number of response alternatives
 * @return true on success
 */
bool cingulate_begin_monitoring(cingulate_adapter_t* adapter, uint32_t num_options);

/**
 * @brief Update response option activation
 *
 * WHAT: Update the activation level of a response option
 * WHY:  Track response competition dynamics
 * HOW:  Store activation, check for conflict conditions
 *
 * @param adapter Adapter instance
 * @param option Response option with updated activation
 * @return true on success
 */
bool cingulate_update_response(cingulate_adapter_t* adapter,
                                const cingulate_response_option_t* option);

/**
 * @brief Evaluate current conflict level
 *
 * WHAT: Compute conflict between competing responses
 * WHY:  Detect need for cognitive control
 * HOW:  Use Botvinick's conflict model (product of activations)
 *
 * BIOLOGICAL: Based on Botvinick et al. (2001) conflict monitoring model:
 * Conflict = sum(activation_i * activation_j) for i != j
 *
 * @param adapter Adapter instance
 * @param conflict Output: detected conflict (if any)
 * @return true if conflict detected above threshold
 */
bool cingulate_evaluate_conflict(cingulate_adapter_t* adapter,
                                  cingulate_conflict_t* conflict);

/**
 * @brief Check if conflict requires control
 *
 * WHAT: Determine if executive control intervention is needed
 * WHY:  Only escalate significant conflicts
 * HOW:  Compare conflict level to threshold
 *
 * @param adapter Adapter instance
 * @param conflict Conflict to evaluate
 * @return true if control intervention needed
 */
bool cingulate_requires_control(const cingulate_adapter_t* adapter,
                                 const cingulate_conflict_t* conflict);

/*=============================================================================
 * ERROR DETECTION (ACC - Rostral)
 *===========================================================================*/

/**
 * @brief Report executed response
 *
 * WHAT: Report which response was actually executed
 * WHY:  Compare to intended response for error detection
 * HOW:  Store executed response, trigger error processing if mismatch
 *
 * @param adapter Adapter instance
 * @param executed_option ID of executed response
 * @param intended_option ID of intended response (0 if unknown)
 * @return true on success
 */
bool cingulate_report_response(cingulate_adapter_t* adapter,
                                uint32_t executed_option,
                                uint32_t intended_option);

/**
 * @brief Report outcome feedback
 *
 * WHAT: Provide feedback about response outcome
 * WHY:  Detect errors from negative outcomes
 * HOW:  Compare outcome to expectation, generate ERN if error
 *
 * @param adapter Adapter instance
 * @param outcome Actual outcome value
 * @param expected Expected outcome value
 * @return true on success
 */
bool cingulate_report_outcome(cingulate_adapter_t* adapter,
                               float outcome,
                               float expected);

/**
 * @brief Get most recent error event
 *
 * WHAT: Retrieve the most recently detected error
 * WHY:  Allow other modules to respond to errors
 * HOW:  Return cached error event
 *
 * @param adapter Adapter instance
 * @param error Output: error event
 * @return true if error available
 */
bool cingulate_get_last_error(const cingulate_adapter_t* adapter,
                               cingulate_error_event_t* error);

/**
 * @brief Check if error reached awareness
 *
 * WHAT: Determine if error was consciously perceived
 * WHY:  Conscious errors drive stronger behavioral adjustment
 * HOW:  Check Pe amplitude (positivity error, ~200-500ms post-error)
 *
 * @param adapter Adapter instance
 * @param error_id Error to check
 * @return true if error was consciously perceived
 */
bool cingulate_error_is_conscious(const cingulate_adapter_t* adapter,
                                   uint32_t error_id);

/*=============================================================================
 * COGNITIVE CONTROL (ACC Output)
 *===========================================================================*/

/**
 * @brief Generate cognitive control signal
 *
 * WHAT: Produce control adjustment based on conflict/error
 * WHY:  Enable adaptive behavioral adjustment
 * HOW:  Compute control level from recent conflicts and errors
 *
 * @param adapter Adapter instance
 * @param signal Output: control signal
 * @return true on success
 */
bool cingulate_generate_control_signal(cingulate_adapter_t* adapter,
                                        cingulate_control_signal_t* signal);

/**
 * @brief Set control callback
 *
 * WHAT: Register callback for control signal generation
 * WHY:  Allow executive functions to receive control signals
 * HOW:  Store callback, invoke on control generation
 *
 * @param adapter Adapter instance
 * @param callback Control signal handler
 * @param user_data User context
 * @return true on success
 */
bool cingulate_set_control_callback(cingulate_adapter_t* adapter,
                                     cingulate_control_callback_t callback,
                                     void* user_data);

/**
 * @brief Get current cognitive control level
 *
 * WHAT: Return current top-down control level
 * WHY:  Other modules can modulate behavior based on control
 * HOW:  Return cached control level
 *
 * @param adapter Adapter instance
 * @return Control level [0, 1], 0 on error
 */
float cingulate_get_control_level(const cingulate_adapter_t* adapter);

/*=============================================================================
 * SELF-REFERENTIAL PROCESSING (PCC)
 *===========================================================================*/

/**
 * @brief Evaluate self-relevance of stimulus
 *
 * WHAT: Assess how self-relevant a stimulus is
 * WHY:  PCC activates for self-referential processing
 * HOW:  Compare stimulus features to self-model
 *
 * @param adapter Adapter instance
 * @param features Feature vector to evaluate
 * @param num_features Number of features
 * @param result Output: self-reference result
 * @return true on success
 */
bool cingulate_evaluate_self_relevance(cingulate_adapter_t* adapter,
                                        const float* features,
                                        uint32_t num_features,
                                        cingulate_self_reference_t* result);

/**
 * @brief Check if in Default Mode Network state
 *
 * WHAT: Determine if PCC is in DMN/mind-wandering state
 * WHY:  DMN competes with task-positive networks
 * HOW:  Check internal focus level and autobio activation
 *
 * @param adapter Adapter instance
 * @return true if in DMN state
 */
bool cingulate_is_default_mode(const cingulate_adapter_t* adapter);

/**
 * @brief Request autobiographical memory access
 *
 * WHAT: Request retrieval of related autobiographical memory
 * WHY:  PCC mediates autobiographical memory access
 * HOW:  Send request to autobiographical memory system
 *
 * @param adapter Adapter instance
 * @param query_features Feature query
 * @param num_features Number of features
 * @param memory_id Output: associated memory ID
 * @return true if memory found
 */
bool cingulate_request_autobio_memory(cingulate_adapter_t* adapter,
                                       const float* query_features,
                                       uint32_t num_features,
                                       uint32_t* memory_id);

/*=============================================================================
 * EMOTION-COGNITION INTEGRATION
 *===========================================================================*/

/**
 * @brief Integrate emotional signal
 *
 * WHAT: Incorporate emotional valence into conflict processing
 * WHY:  Rostral ACC integrates emotion with cognition
 * HOW:  Modulate conflict threshold based on emotional state
 *
 * @param adapter Adapter instance
 * @param valence Emotional valence [-1, 1] (negative to positive)
 * @param arousal Emotional arousal [0, 1]
 * @return true on success
 */
bool cingulate_integrate_emotion(cingulate_adapter_t* adapter,
                                  float valence,
                                  float arousal);

/**
 * @brief Report pain/distress signal
 *
 * WHAT: Process pain or distress signal through rostral ACC
 * WHY:  ACC is involved in pain processing and distress signaling
 * HOW:  Update distress level, potentially trigger control signal
 *
 * @param adapter Adapter instance
 * @param pain_level Pain intensity [0, 1]
 * @param is_physical true if physical pain, false if social/emotional
 * @return true on success
 */
bool cingulate_report_pain(cingulate_adapter_t* adapter,
                            float pain_level,
                            bool is_physical);

/*=============================================================================
 * STATUS AND DIAGNOSTICS
 *===========================================================================*/

/**
 * @brief Get current processing status
 *
 * @param adapter Adapter instance
 * @return Current status
 */
cingulate_status_t cingulate_get_status(const cingulate_adapter_t* adapter);

/**
 * @brief Get last error code
 *
 * @param adapter Adapter instance
 * @return Last error, or CINGULATE_ERROR_NONE
 */
cingulate_error_t cingulate_get_last_error_code(const cingulate_adapter_t* adapter);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* cingulate_error_string(cingulate_error_t error);

/**
 * @brief Get status description string
 *
 * @param status Status code
 * @return Human-readable status description
 */
const char* cingulate_status_string(cingulate_status_t status);

/**
 * @brief Get adapter statistics
 *
 * @param adapter Adapter instance
 * @param stats Output statistics structure
 * @return true on success
 */
bool cingulate_get_stats(const cingulate_adapter_t* adapter, cingulate_stats_t* stats);

/**
 * @brief Get adapter configuration
 *
 * @param adapter Adapter instance
 * @param config Output configuration structure
 * @return true on success
 */
bool cingulate_get_config(const cingulate_adapter_t* adapter, cingulate_config_t* config);

/*=============================================================================
 * CALLBACK REGISTRATION
 *===========================================================================*/

/**
 * @brief Set conflict callback
 *
 * @param adapter Adapter instance
 * @param callback Conflict handler function
 * @param user_data User context
 * @return true on success
 */
bool cingulate_set_conflict_callback(cingulate_adapter_t* adapter,
                                      cingulate_conflict_callback_t callback,
                                      void* user_data);

/**
 * @brief Set error callback
 *
 * @param adapter Adapter instance
 * @param callback Error handler function
 * @param user_data User context
 * @return true on success
 */
bool cingulate_set_error_callback(cingulate_adapter_t* adapter,
                                   cingulate_error_callback_t callback,
                                   void* user_data);

/**
 * @brief Set event callback
 *
 * @param adapter Adapter instance
 * @param callback Event handler function
 * @param user_data User context
 * @return true on success
 */
bool cingulate_set_event_callback(cingulate_adapter_t* adapter,
                                   cingulate_event_callback_t callback,
                                   void* user_data);

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/**
 * @brief Get bio-async module context
 *
 * WHAT: Returns the bio-async module context for cingulate
 * WHY:  Allow external modules to send messages to cingulate
 * HOW:  Returns internal bio_module_context_t
 *
 * @param adapter Adapter instance
 * @return Bio-async module context, or NULL if not enabled
 */
bio_module_context_t cingulate_get_bio_context(cingulate_adapter_t* adapter);

/**
 * @brief Process pending bio-async messages
 *
 * WHAT: Process messages in cingulate's inbox
 * WHY:  Handle incoming requests from other modules
 * HOW:  Calls bio_router_process_inbox and invokes handlers
 *
 * @param adapter Adapter instance
 * @param max_messages Maximum messages to process (0 = all)
 * @return Number of messages processed
 */
uint32_t cingulate_process_bio_messages(cingulate_adapter_t* adapter, uint32_t max_messages);

/**
 * @brief Broadcast conflict detected
 *
 * WHAT: Notify all modules of detected conflict
 * WHY:  Allow system-wide response to conflict
 * HOW:  Broadcasts conflict event via bio-async
 *
 * @param adapter Adapter instance
 * @param conflict Conflict to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cingulate_broadcast_conflict(cingulate_adapter_t* adapter,
                                            const cingulate_conflict_t* conflict);

/**
 * @brief Broadcast error detected
 *
 * WHAT: Notify all modules of detected error
 * WHY:  Allow system-wide error response
 * HOW:  Broadcasts error event via bio-async
 *
 * @param adapter Adapter instance
 * @param error Error to broadcast
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t cingulate_broadcast_error(cingulate_adapter_t* adapter,
                                         const cingulate_error_event_t* error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CINGULATE_ADAPTER_H */
