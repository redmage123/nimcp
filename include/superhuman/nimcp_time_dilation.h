/**
 * @file nimcp_time_dilation.h
 * @brief Subjective Time Manipulation - Superhuman Temporal Processing Module
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Subjective time manipulation through processing speed modulation
 * WHY:  Enable enhanced reaction time and temporal resolution
 * HOW:  Variable processing rate, temporal buffering, "bullet time" simulation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * TIME PERCEPTION NEUROSCIENCE:
 * ----------------------------
 * 1. Subjective Time Dilation:
 *    - Novel/threatening stimuli elongate perceived time
 *    - Tachypsychia: time slows during life-threatening situations
 *    - Increased norepinephrine enhances memory encoding rate
 *    - Reference: Stetson et al. (2007) "Does time really slow down during
 *      a frightening event?"
 *
 * 2. Neural Timing Circuits:
 *    - Striatal beat frequency model for interval timing
 *    - Dopamine modulates clock speed
 *    - Cerebellum: sub-second timing precision
 *    - Reference: Meck (2005) "Neuropsychology of timing and time perception"
 *
 * 3. Temporal Resolution:
 *    - Conscious perception: ~30-50ms integration window
 *    - Athletes show ~10-20% faster temporal discrimination
 *    - Training improves temporal resolution
 *    - Reference: Yarrow et al. (2009) "The privileged status of touch"
 *
 * ADRENALINE RESPONSE:
 * -------------------
 * 1. Fight-or-Flight Enhancement:
 *    - Increased visual processing bandwidth
 *    - Faster motor preparation
 *    - Enhanced working memory encoding
 *    - Time compression during high arousal
 *
 * 2. Neural Acceleration:
 *    - Increased gamma oscillations (40-100Hz)
 *    - Enhanced signal transmission speed
 *    - Reduced inhibitory delays
 *
 * IMPLEMENTATION:
 * ---------------
 * - Processing speed multiplier (1x-10x subjective)
 * - Input buffering for oversampling
 * - Temporal resolution enhancement
 * - Reaction time optimization
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_TIME_DILATION_H
#define NIMCP_TIME_DILATION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TIME_DILATION_MAX_FACTOR        10.0f   /**< Maximum time dilation (10x slower) */
#define TIME_DILATION_MIN_FACTOR        0.5f    /**< Minimum time dilation (2x faster) */
#define TIME_DILATION_DEFAULT           1.0f    /**< Normal time (no dilation) */
#define TIME_DILATION_BUFFER_SIZE       1024    /**< Default temporal buffer size */
#define TIME_DILATION_MAX_EVENTS        256     /**< Maximum tracked events */
#define TIME_DILATION_HISTORY_MS        1000    /**< Event history window (ms) */
#define TIME_DILATION_RESOLUTION_US     100     /**< Base temporal resolution (us) */

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    TIME_DILATION_SUCCESS                = 0,
    TIME_DILATION_ERROR_NULL_POINTER     = -1,
    TIME_DILATION_ERROR_INVALID_PARAM    = -2,
    TIME_DILATION_ERROR_NO_MEMORY        = -3,
    TIME_DILATION_ERROR_NOT_INITIALIZED  = -4,
    TIME_DILATION_ERROR_INVALID_STATE    = -5,
    TIME_DILATION_ERROR_BUFFER_FULL      = -6,
    TIME_DILATION_ERROR_BUFFER_EMPTY     = -7,
    TIME_DILATION_ERROR_LIMIT_EXCEEDED   = -8,
    TIME_DILATION_ERROR_RESOURCE_EXHAUSTED = -9
} time_dilation_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Time dilation mode
 */
typedef enum {
    TIME_MODE_NORMAL,       /**< Normal processing speed */
    TIME_MODE_ACCELERATED,  /**< Faster subjective time (2-4x) */
    TIME_MODE_BULLET_TIME,  /**< Maximum dilation (5-10x) */
    TIME_MODE_ADAPTIVE,     /**< Automatic based on context */
    TIME_MODE_CUSTOM        /**< Custom dilation factor */
} time_dilation_mode_t;

/**
 * @brief Trigger for automatic time dilation
 */
typedef enum {
    TIME_TRIGGER_NONE,          /**< No automatic trigger */
    TIME_TRIGGER_THREAT,        /**< Detected threat/danger */
    TIME_TRIGGER_NOVELTY,       /**< Novel stimulus detected */
    TIME_TRIGGER_COMPLEXITY,    /**< High cognitive load */
    TIME_TRIGGER_MOTION,        /**< Fast-moving object */
    TIME_TRIGGER_DECISION,      /**< Critical decision point */
    TIME_TRIGGER_MANUAL         /**< Manual activation */
} time_dilation_trigger_t;

/**
 * @brief Event priority for processing
 */
typedef enum {
    TIME_PRIORITY_LOW,          /**< Background processing */
    TIME_PRIORITY_NORMAL,       /**< Standard priority */
    TIME_PRIORITY_HIGH,         /**< Elevated priority */
    TIME_PRIORITY_CRITICAL      /**< Immediate processing */
} time_event_priority_t;

/**
 * @brief Temporal resolution level
 */
typedef enum {
    TIME_RESOLUTION_COARSE,     /**< ~100ms resolution */
    TIME_RESOLUTION_NORMAL,     /**< ~30ms resolution */
    TIME_RESOLUTION_FINE,       /**< ~10ms resolution */
    TIME_RESOLUTION_ULTRA       /**< ~1ms resolution */
} time_resolution_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Temporal event for processing
 */
typedef struct {
    uint32_t event_id;          /**< Unique event identifier */
    uint64_t real_time_us;      /**< Real world timestamp (microseconds) */
    uint64_t subjective_time_us; /**< Subjective timestamp */
    float* data;                /**< Event data (caller owns) */
    uint32_t data_size;         /**< Data size in floats */
    time_event_priority_t priority; /**< Event priority */
    bool processed;             /**< Processing complete */
    float processing_deadline_ms; /**< Real-time deadline */
} time_event_t;

/**
 * @brief Reaction time measurement
 */
typedef struct {
    float stimulus_time_ms;     /**< Stimulus presentation time */
    float response_time_ms;     /**< Response initiation time */
    float reaction_time_ms;     /**< Total reaction time */
    float motor_time_ms;        /**< Motor execution time */
    float decision_time_ms;     /**< Decision-making time */
    float dilation_factor;      /**< Active dilation during reaction */
    bool is_accurate;           /**< Response accuracy */
} time_reaction_t;

/**
 * @brief Temporal buffer for oversampling
 */
typedef struct {
    float* samples;             /**< Sample buffer */
    uint64_t* timestamps_us;    /**< Sample timestamps */
    uint32_t capacity;          /**< Buffer capacity */
    uint32_t count;             /**< Current sample count */
    uint32_t head;              /**< Circular buffer head */
    uint32_t tail;              /**< Circular buffer tail */
    float sample_rate;          /**< Current sample rate */
} time_buffer_t;

/**
 * @brief Time dilation configuration
 */
typedef struct {
    /* Dilation settings */
    time_dilation_mode_t mode;          /**< Dilation mode */
    float base_dilation_factor;         /**< Base dilation factor [0.5-10] */
    float max_dilation_factor;          /**< Maximum allowed dilation */
    float min_dilation_factor;          /**< Minimum allowed dilation */
    float dilation_ramp_rate;           /**< Rate of dilation change (per second) */

    /* Resolution settings */
    time_resolution_t resolution;       /**< Temporal resolution level */
    float resolution_us;                /**< Custom resolution (microseconds) */

    /* Trigger settings */
    bool enable_auto_triggers;          /**< Enable automatic dilation */
    float threat_threshold;             /**< Threat detection threshold */
    float novelty_threshold;            /**< Novelty detection threshold */
    float complexity_threshold;         /**< Cognitive load threshold */

    /* Buffer settings */
    uint32_t input_buffer_size;         /**< Input buffer capacity */
    uint32_t output_buffer_size;        /**< Output buffer capacity */
    bool enable_interpolation;          /**< Enable temporal interpolation */

    /* Resource limits */
    float max_processing_load;          /**< Maximum CPU load [0-1] */
    uint64_t max_dilation_duration_ms;  /**< Maximum continuous dilation */
    float energy_budget;                /**< Computational energy budget */

    /* Reaction optimization */
    bool enable_prediction;             /**< Enable predictive processing */
    bool enable_precomputation;         /**< Enable result precomputation */
    float prediction_horizon_ms;        /**< Prediction time horizon */
} time_dilation_config_t;

/**
 * @brief Current dilation state
 */
typedef struct {
    /* Active dilation */
    float current_factor;               /**< Current dilation factor */
    float target_factor;                /**< Target dilation factor */
    time_dilation_mode_t active_mode;   /**< Currently active mode */
    time_dilation_trigger_t last_trigger; /**< Most recent trigger */

    /* Timing */
    uint64_t real_time_us;              /**< Real world time */
    uint64_t subjective_time_us;        /**< Subjective time */
    uint64_t dilation_start_us;         /**< When dilation started */
    uint64_t dilation_duration_us;      /**< How long dilated */

    /* Events */
    uint32_t pending_events;            /**< Events awaiting processing */
    uint32_t processed_events;          /**< Events processed this cycle */
    float event_throughput;             /**< Events per real second */

    /* Resources */
    float processing_load;              /**< Current processing load [0-1] */
    float energy_consumed;              /**< Energy used */
    float energy_remaining;             /**< Energy budget remaining */

    /* Resolution */
    float effective_resolution_us;      /**< Achieved temporal resolution */
    bool is_initialized;                /**< System initialized */
    bool is_dilating;                   /**< Currently in dilation */
} time_dilation_state_t;

/**
 * @brief Accumulated statistics
 */
typedef struct {
    /* Dilation statistics */
    uint64_t total_dilation_events;     /**< Times dilation activated */
    uint64_t total_dilation_time_us;    /**< Total time dilated */
    float avg_dilation_factor;          /**< Average dilation factor */
    float max_dilation_achieved;        /**< Peak dilation reached */
    float min_dilation_achieved;        /**< Minimum dilation (acceleration) */

    /* Event statistics */
    uint64_t total_events_processed;    /**< All events processed */
    float avg_event_latency_us;         /**< Average event latency */
    float min_event_latency_us;         /**< Minimum achieved latency */
    uint64_t deadline_misses;           /**< Events missing deadline */
    float deadline_hit_rate;            /**< Deadline success rate */

    /* Reaction statistics */
    uint64_t total_reactions;           /**< Reactions measured */
    float avg_reaction_time_ms;         /**< Average reaction time */
    float best_reaction_time_ms;        /**< Best reaction time */
    float reaction_improvement;         /**< Improvement vs baseline */

    /* Performance statistics */
    float avg_processing_load;          /**< Average CPU load */
    float peak_processing_load;         /**< Peak CPU load */
    float total_energy_consumed;        /**< Total energy used */
    uint64_t resolution_adjustments;    /**< Resolution changes made */

    /* Trigger statistics */
    uint64_t threat_triggers;           /**< Threat-triggered dilations */
    uint64_t novelty_triggers;          /**< Novelty-triggered dilations */
    uint64_t manual_triggers;           /**< Manual activations */
} time_dilation_stats_t;

/**
 * @brief Opaque time dilation system handle
 */
typedef struct time_dilation_system time_dilation_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize config with biologically-plausible defaults
 * WHY:  Provide sensible starting point for time dilation
 * HOW:  Set all fields to validated default values
 *
 * @param config Output configuration structure
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_default_config(time_dilation_config_t* config);

/**
 * @brief Create time dilation system
 *
 * WHAT: Allocate and initialize temporal processing system
 * WHY:  Enable subjective time manipulation
 * HOW:  Allocate buffers, initialize timing, setup triggers
 *
 * @param config Configuration (NULL for defaults)
 * @return New time dilation system or NULL on failure
 */
time_dilation_system_t* time_dilation_create(const time_dilation_config_t* config);

/**
 * @brief Destroy time dilation system
 *
 * WHAT: Release all resources
 * WHY:  Clean shutdown and memory management
 * HOW:  Free buffers, events, timing state
 *
 * @param system System to destroy (NULL-safe)
 */
void time_dilation_destroy(time_dilation_system_t* system);

/**
 * @brief Reset system to initial state
 *
 * WHAT: Clear all events and restore normal time
 * WHY:  Prepare for new session
 * HOW:  Zero state, clear buffers, reset to 1x time
 *
 * @param system System to reset
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_reset(time_dilation_system_t* system);

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Update system configuration
 *
 * @param system Active system
 * @param config New configuration
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_set_config(time_dilation_system_t* system,
                             const time_dilation_config_t* config);

/**
 * @brief Get current configuration
 *
 * @param system Active system
 * @param config Output configuration
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_config(const time_dilation_system_t* system,
                             time_dilation_config_t* config);

/**
 * @brief Set dilation mode
 *
 * @param system Active system
 * @param mode New dilation mode
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_set_mode(time_dilation_system_t* system,
                           time_dilation_mode_t mode);

/**
 * @brief Set custom dilation factor
 *
 * @param system Active system
 * @param factor Dilation factor [0.5-10.0]
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_set_factor(time_dilation_system_t* system, float factor);

/**
 * @brief Set temporal resolution
 *
 * @param system Active system
 * @param resolution Resolution level
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_set_resolution(time_dilation_system_t* system,
                                 time_resolution_t resolution);

/* ============================================================================
 * Time Dilation Control API
 * ============================================================================ */

/**
 * @brief Activate time dilation (bullet time)
 *
 * WHAT: Begin subjective time dilation
 * WHY:  Enable enhanced temporal processing
 * HOW:  Ramp up dilation factor, increase processing rate
 *
 * @param system Active system
 * @param trigger Reason for activation
 * @param factor Dilation factor (0 = use config default)
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_activate(time_dilation_system_t* system,
                           time_dilation_trigger_t trigger,
                           float factor);

/**
 * @brief Deactivate time dilation
 *
 * WHAT: Return to normal time
 * WHY:  End enhanced processing, conserve resources
 * HOW:  Ramp down dilation factor to 1.0
 *
 * @param system Active system
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_deactivate(time_dilation_system_t* system);

/**
 * @brief Process automatic triggers
 *
 * WHAT: Check for automatic dilation triggers
 * WHY:  Enable reactive time dilation
 * HOW:  Evaluate threat, novelty, complexity thresholds
 *
 * @param system Active system
 * @param threat_level Current threat assessment [0-1]
 * @param novelty_level Current novelty level [0-1]
 * @param complexity_level Current cognitive load [0-1]
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_check_triggers(time_dilation_system_t* system,
                                 float threat_level,
                                 float novelty_level,
                                 float complexity_level);

/* ============================================================================
 * Event Processing API
 * ============================================================================ */

/**
 * @brief Submit event for temporal processing
 *
 * WHAT: Queue event for dilated-time processing
 * WHY:  Enable enhanced analysis of time-critical events
 * HOW:  Add to temporal buffer with appropriate timestamp
 *
 * @param system Active system
 * @param data Event data array
 * @param data_size Data size in floats
 * @param priority Event priority
 * @param deadline_ms Real-time processing deadline (0 = no deadline)
 * @param event_id Output: assigned event ID
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_submit_event(time_dilation_system_t* system,
                               const float* data,
                               uint32_t data_size,
                               time_event_priority_t priority,
                               float deadline_ms,
                               uint32_t* event_id);

/**
 * @brief Process pending events
 *
 * WHAT: Process events in dilated time
 * WHY:  Main processing loop for time dilation
 * HOW:  Process events at enhanced temporal resolution
 *
 * @param system Active system
 * @param real_delta_ms Real time since last call
 * @param events_processed Output: number of events processed
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_process(time_dilation_system_t* system,
                          float real_delta_ms,
                          uint32_t* events_processed);

/**
 * @brief Get next processed event
 *
 * @param system Active system
 * @param event Output event structure
 * @return TIME_DILATION_SUCCESS, TIME_DILATION_ERROR_BUFFER_EMPTY, or error
 */
int time_dilation_get_event(time_dilation_system_t* system,
                            time_event_t* event);

/**
 * @brief Clear all pending events
 *
 * @param system Active system
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_clear_events(time_dilation_system_t* system);

/* ============================================================================
 * Temporal Conversion API
 * ============================================================================ */

/**
 * @brief Convert real time to subjective time
 *
 * WHAT: Transform real-world time to dilated time
 * WHY:  Coordinate with external timing
 * HOW:  Apply current dilation factor
 *
 * @param system Active system
 * @param real_time_us Real time in microseconds
 * @param subjective_time_us Output subjective time
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_real_to_subjective(time_dilation_system_t* system,
                                     uint64_t real_time_us,
                                     uint64_t* subjective_time_us);

/**
 * @brief Convert subjective time to real time
 *
 * @param system Active system
 * @param subjective_time_us Subjective time in microseconds
 * @param real_time_us Output real time
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_subjective_to_real(time_dilation_system_t* system,
                                     uint64_t subjective_time_us,
                                     uint64_t* real_time_us);

/**
 * @brief Get current subjective time
 *
 * @param system Active system
 * @param subjective_time_us Output current subjective time
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_subjective_time(time_dilation_system_t* system,
                                      uint64_t* subjective_time_us);

/**
 * @brief Get effective temporal resolution
 *
 * @param system Active system
 * @param resolution_us Output resolution in microseconds
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_resolution(const time_dilation_system_t* system,
                                 float* resolution_us);

/* ============================================================================
 * Reaction Time API
 * ============================================================================ */

/**
 * @brief Start reaction timing
 *
 * WHAT: Begin measuring reaction time
 * WHY:  Track enhanced reaction capabilities
 * HOW:  Record stimulus presentation time
 *
 * @param system Active system
 * @param reaction Output: reaction tracking structure
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_start_reaction(time_dilation_system_t* system,
                                 time_reaction_t* reaction);

/**
 * @brief Record response initiation
 *
 * @param system Active system
 * @param reaction Reaction tracking structure
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_record_response(time_dilation_system_t* system,
                                  time_reaction_t* reaction);

/**
 * @brief Complete reaction timing
 *
 * @param system Active system
 * @param reaction Reaction tracking structure
 * @param is_accurate Whether response was accurate
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_complete_reaction(time_dilation_system_t* system,
                                    time_reaction_t* reaction,
                                    bool is_accurate);

/**
 * @brief Get average reaction time
 *
 * @param system Active system
 * @param avg_ms Output average reaction time (ms)
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_avg_reaction(const time_dilation_system_t* system,
                                   float* avg_ms);

/* ============================================================================
 * State and Statistics API
 * ============================================================================ */

/**
 * @brief Get current dilation state
 *
 * @param system Active system
 * @param state Output state structure
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_state(const time_dilation_system_t* system,
                            time_dilation_state_t* state);

/**
 * @brief Get accumulated statistics
 *
 * @param system Active system
 * @param stats Output statistics structure
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_get_stats(const time_dilation_system_t* system,
                            time_dilation_stats_t* stats);

/**
 * @brief Reset statistics counters
 *
 * @param system Active system
 * @return TIME_DILATION_SUCCESS or error code
 */
int time_dilation_reset_stats(time_dilation_system_t* system);

/* ============================================================================
 * Utility API
 * ============================================================================ */

/**
 * @brief Check if time is currently dilated
 *
 * @param system Active system
 * @return true if dilation active, false otherwise
 */
bool time_dilation_is_active(const time_dilation_system_t* system);

/**
 * @brief Get current dilation factor
 *
 * @param system Active system
 * @return Current dilation factor (1.0 = normal)
 */
float time_dilation_get_factor(const time_dilation_system_t* system);

/**
 * @brief Get error description string
 *
 * @param error Error code
 * @return Human-readable error description
 */
const char* time_dilation_error_string(time_dilation_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TIME_DILATION_H */
