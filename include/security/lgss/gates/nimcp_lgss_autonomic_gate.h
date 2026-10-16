/**
 * @file nimcp_lgss_autonomic_gate.h
 * @brief LGSS Autonomic Output Gate - Final safety barrier for internal state control
 *
 * WHAT: Provides gated control of autonomic functions including hormone release,
 *       vital signs modulation, and internal state regulation. All autonomic
 *       outputs must pass through this gate.
 *
 * WHY:  Autonomic functions affect the fundamental operating state of the system.
 *       Uncontrolled hormone release or vital sign manipulation can destabilize
 *       the entire cognitive architecture. This gate ensures biological homeostasis.
 *
 * HOW:  Implements hormone release limiting, vital signs monitoring, and
 *       homeostatic boundary enforcement. Proposals exceeding safe limits
 *       are clamped or rejected to maintain system stability.
 *
 * BIOLOGICAL BASIS: Analogous to the hypothalamus and autonomic nervous system
 *       with brainstem regulatory centers and hormonal feedback loops.
 */

#ifndef NIMCP_LGSS_AUTONOMIC_GATE_H
#define NIMCP_LGSS_AUTONOMIC_GATE_H

#include "utils/validation/nimcp_common.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * Constants
 * ============================================================================= */

/** Magic number for autonomic gate validation ('AUGT') */
#define NIMCP_AUTONOMIC_GATE_MAGIC 0x41554754

/** Number of hormone types */
#define NIMCP_AUTONOMIC_HORMONE_COUNT 8

/** Default hormone level range */
#define NIMCP_AUTONOMIC_DEFAULT_MIN_LEVEL 0.0f
#define NIMCP_AUTONOMIC_DEFAULT_MAX_LEVEL 1.0f

/** Default heart rate bounds (BPM equivalent for simulation) */
#define NIMCP_AUTONOMIC_DEFAULT_HR_MIN 40.0f
#define NIMCP_AUTONOMIC_DEFAULT_HR_MAX 200.0f

/** Default hormone release rate limit (per second) */
#define NIMCP_AUTONOMIC_DEFAULT_RELEASE_RATE 0.1f

/** Maximum rate of change for vital signs per step */
#define NIMCP_AUTONOMIC_DEFAULT_VITAL_RATE 0.05f

/* =============================================================================
 * Enumerations
 * ============================================================================= */

/**
 * @brief Hormone types for autonomic regulation
 *
 * WHAT: Simulated hormones affecting system behavior
 * WHY:  Hormones modulate learning, attention, stress response
 * HOW:  Release is gated to prevent destabilizing levels
 *
 * BIOLOGICAL BASIS: Mapped to major neuromodulatory systems
 */
typedef enum {
    HORMONE_DOPAMINE = 0,       /**< Reward/motivation modulation */
    HORMONE_SEROTONIN,          /**< Mood/impulse regulation */
    HORMONE_NOREPINEPHRINE,     /**< Alertness/stress response */
    HORMONE_CORTISOL,           /**< Stress hormone */
    HORMONE_OXYTOCIN,           /**< Social bonding/trust */
    HORMONE_MELATONIN,          /**< Sleep/circadian regulation */
    HORMONE_ACETYLCHOLINE,      /**< Attention/memory modulation */
    HORMONE_GABA                /**< Inhibitory regulation */
} autonomic_hormone_t;

/**
 * @brief Vital sign types
 *
 * WHAT: Simulated vital signs representing system state
 * WHY:  Vital signs indicate overall system health
 * HOW:  Monitored to detect abnormal states
 */
typedef enum {
    VITAL_HEART_RATE = 0,       /**< Processing rate (BPM equivalent) */
    VITAL_TEMPERATURE,          /**< System temperature (heat/load) */
    VITAL_RESPIRATION,          /**< I/O throughput (breaths/min equiv) */
    VITAL_BLOOD_PRESSURE_SYS,   /**< System pressure - systolic analog */
    VITAL_BLOOD_PRESSURE_DIA,   /**< System pressure - diastolic analog */
    VITAL_STRESS_LEVEL,         /**< Overall stress indicator */
    VITAL_ENERGY_LEVEL,         /**< Available energy/resources */
    VITAL_COUNT                 /**< Total number of vital signs */
} autonomic_vital_t;

/**
 * @brief Autonomic gate release result
 */
typedef enum {
    AUTONOMIC_RELEASE_SUCCESS = 0,   /**< Release executed successfully */
    AUTONOMIC_RELEASE_CLAMPED,       /**< Release clamped to limit */
    AUTONOMIC_RELEASE_RATE_LIMITED,  /**< Release rate-limited */
    AUTONOMIC_RELEASE_BLOCKED,       /**< Release blocked entirely */
    AUTONOMIC_RELEASE_HORMONE_LOCKED, /**< Hormone type is locked */
    AUTONOMIC_RELEASE_GATE_DISABLED, /**< Autonomic gate is disabled */
    AUTONOMIC_RELEASE_INVALID,       /**< Invalid release request */
    AUTONOMIC_RELEASE_ERROR          /**< General release error */
} autonomic_release_result_t;

/**
 * @brief Vital sign status
 */
typedef enum {
    VITAL_STATUS_NORMAL = 0,    /**< Within normal range */
    VITAL_STATUS_LOW,           /**< Below normal range */
    VITAL_STATUS_HIGH,          /**< Above normal range */
    VITAL_STATUS_CRITICAL_LOW,  /**< Critically low */
    VITAL_STATUS_CRITICAL_HIGH  /**< Critically high */
} autonomic_vital_status_t;

/* =============================================================================
 * Structures
 * ============================================================================= */

/**
 * @brief Hormone release limits
 *
 * WHAT: Defines safe operating limits for each hormone
 * WHY:  Prevents destabilizing hormone levels
 * HOW:  Release requests are clamped or rejected based on limits
 */
typedef struct {
    float min_level;             /**< Minimum allowed level (0.0-1.0) */
    float max_level;             /**< Maximum allowed level (0.0-1.0) */
    float max_release_rate;      /**< Max release per time step */
    float max_absorption_rate;   /**< Max absorption/decay per time step */
    float target_baseline;       /**< Target baseline level for homeostasis */
    bool enabled;                /**< Whether this hormone is enabled */
    bool locked;                 /**< Whether changes are locked */
} autonomic_hormone_limits_t;

/**
 * @brief Vital sign bounds
 *
 * WHAT: Defines normal and critical ranges for vital signs
 * WHY:  Enables detection of abnormal system states
 * HOW:  Vital signs outside bounds trigger alerts/actions
 */
typedef struct {
    float normal_min;            /**< Lower bound of normal range */
    float normal_max;            /**< Upper bound of normal range */
    float critical_min;          /**< Critical low threshold */
    float critical_max;          /**< Critical high threshold */
    float max_rate_of_change;    /**< Max change per time step */
    bool monitoring_enabled;     /**< Whether monitoring is active */
} autonomic_vital_bounds_t;

/**
 * @brief Hormone release request
 *
 * WHAT: A request to release a hormone amount
 * WHY:  All hormone changes must be validated through the gate
 * HOW:  Request is checked against limits before execution
 */
typedef struct {
    autonomic_hormone_t hormone; /**< Target hormone type */
    float amount;                /**< Amount to release (can be negative for reduction) */
    float duration;              /**< Duration of release effect (seconds) */
    bool immediate;              /**< Whether to apply immediately or gradually */
    uint64_t timestamp;          /**< Request timestamp */
    uint32_t sequence_id;        /**< Unique sequence identifier */
    const char* reason;          /**< Reason for release (for logging) */
} autonomic_release_request_t;

/**
 * @brief Current hormone state
 *
 * WHAT: Current levels and rates for all hormones
 * WHY:  Enables monitoring of hormone state
 * HOW:  Updated after each release/absorption cycle
 */
typedef struct {
    float levels[NIMCP_AUTONOMIC_HORMONE_COUNT];      /**< Current hormone levels */
    float rates[NIMCP_AUTONOMIC_HORMONE_COUNT];       /**< Current rate of change */
    uint64_t last_update[NIMCP_AUTONOMIC_HORMONE_COUNT]; /**< Last update timestamps */
    bool locked[NIMCP_AUTONOMIC_HORMONE_COUNT];       /**< Lock status per hormone */
} autonomic_hormone_state_t;

/**
 * @brief Current vital signs reading
 *
 * WHAT: Current values and status for all vital signs
 * WHY:  Provides snapshot of system health
 * HOW:  Populated by vital signs monitoring
 */
typedef struct {
    float values[VITAL_COUNT];                /**< Current vital sign values */
    autonomic_vital_status_t status[VITAL_COUNT]; /**< Status per vital sign */
    uint64_t timestamp;                       /**< Reading timestamp */
    bool any_critical;                        /**< True if any vital is critical */
    uint32_t critical_count;                  /**< Number of critical vitals */
} autonomic_vitals_reading_t;

/**
 * @brief Autonomic gate release details
 *
 * WHAT: Detailed information about release result
 * WHY:  Enables debugging and audit logging
 * HOW:  Populated when release functions are called
 */
typedef struct {
    autonomic_release_result_t result;  /**< Release result */
    autonomic_hormone_t hormone;        /**< Target hormone */
    float requested_amount;             /**< Originally requested amount */
    float actual_amount;                /**< Actually released amount */
    float new_level;                    /**< New hormone level after release */
    char description[256];              /**< Human-readable description */
    uint64_t timestamp;                 /**< When release occurred */
} autonomic_release_details_t;

/**
 * @brief Autonomic gate statistics
 *
 * WHAT: Operational statistics for the autonomic gate
 * WHY:  Enables monitoring and tuning
 * HOW:  Updated atomically during gate operations
 */
typedef struct {
    uint64_t release_requests;          /**< Total release requests */
    uint64_t releases_approved;         /**< Requests approved as-is */
    uint64_t releases_clamped;          /**< Requests clamped to limits */
    uint64_t releases_blocked;          /**< Requests blocked entirely */
    uint64_t vital_readings;            /**< Total vital sign readings */
    uint64_t vital_alerts;              /**< Vital sign alerts triggered */
    uint64_t critical_events;           /**< Critical state events */
    uint64_t homeostasis_corrections;   /**< Automatic homeostasis corrections */
    float avg_dopamine_level;           /**< Average dopamine level */
    float avg_stress_level;             /**< Average stress vital */
} autonomic_gate_stats_t;

/**
 * @brief Vital sign alert callback type
 *
 * WHAT: Function pointer for vital sign alerts
 * WHY:  Allows external handling of abnormal vitals
 * HOW:  Called when vital signs enter abnormal state
 *
 * @param vital The vital sign that triggered alert
 * @param status Current status of the vital
 * @param value Current value of the vital
 * @param user_data User-provided context data
 */
typedef void (*autonomic_vital_callback_t)(
    autonomic_vital_t vital,
    autonomic_vital_status_t status,
    float value,
    void* user_data
);

/**
 * @brief Homeostasis correction callback type
 *
 * WHAT: Function pointer for homeostasis corrections
 * WHY:  Allows notification of automatic corrections
 * HOW:  Called when gate performs automatic adjustment
 *
 * @param hormone Hormone being corrected
 * @param old_level Previous level
 * @param new_level New level after correction
 * @param user_data User-provided context data
 */
typedef void (*autonomic_homeostasis_callback_t)(
    autonomic_hormone_t hormone,
    float old_level,
    float new_level,
    void* user_data
);

/** Forward declaration */
typedef struct autonomic_gate autonomic_gate_t;

/**
 * @brief Autonomic gate configuration
 */
typedef struct {
    bool enable_homeostasis;            /**< Enable automatic homeostasis */
    float homeostasis_strength;         /**< How strongly to correct (0.0-1.0) */
    bool enable_vital_monitoring;       /**< Enable vital signs monitoring */
    float vital_update_interval_ms;     /**< How often to update vitals */
    autonomic_vital_callback_t vital_callback; /**< Vital alert callback */
    autonomic_homeostasis_callback_t homeostasis_callback; /**< Homeostasis callback */
    void* callback_user_data;           /**< User data for callbacks */
    bool log_all_releases;              /**< Log all releases (debug mode) */
    bool strict_mode;                   /**< Block any borderline requests */
} autonomic_gate_config_t;

/* =============================================================================
 * Function Declarations
 * ============================================================================= */

/**
 * @brief Create an autonomic gate instance
 *
 * WHAT: Allocates and initializes a new autonomic gate
 * WHY:  Autonomic gate is required for safe internal state control
 * HOW:  Allocates memory, initializes default limits
 *
 * @param config Optional configuration (NULL for defaults)
 * @return New autonomic gate instance, or NULL on failure
 */
autonomic_gate_t* autonomic_gate_create(const autonomic_gate_config_t* config);

/**
 * @brief Destroy an autonomic gate instance
 *
 * WHAT: Deallocates an autonomic gate and all associated resources
 * WHY:  Proper cleanup prevents memory leaks
 * HOW:  Frees all internal structures and the gate itself
 *
 * @param gate Autonomic gate to destroy
 */
void autonomic_gate_destroy(autonomic_gate_t* gate);

/**
 * @brief Release a hormone through the gate
 *
 * WHAT: Validates and executes a hormone release request
 * WHY:  This is the ONLY path through which hormones should be released
 * HOW:  Validates against limits, applies clamping if needed, executes
 *
 * @param gate Autonomic gate instance
 * @param request Hormone release request
 * @param details Output: release details (can be NULL)
 * @return Release result code
 */
autonomic_release_result_t autonomic_gate_release_hormone(
    autonomic_gate_t* gate,
    const autonomic_release_request_t* request,
    autonomic_release_details_t* details
);

/**
 * @brief Set hormone limits
 *
 * WHAT: Configures limits for a specific hormone
 * WHY:  Different hormones have different safe operating ranges
 * HOW:  Stores limits for validation during release
 *
 * @param gate Autonomic gate instance
 * @param hormone Target hormone type
 * @param limits Limits to apply
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_set_hormone_limits(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone,
    const autonomic_hormone_limits_t* limits
);

/**
 * @brief Get hormone limits
 *
 * WHAT: Retrieves current limits for a hormone
 * WHY:  Allows inspection of current limits
 * HOW:  Copies current limits to output structure
 *
 * @param gate Autonomic gate instance
 * @param hormone Target hormone type
 * @param limits Output buffer for limits
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_get_hormone_limits(
    const autonomic_gate_t* gate,
    autonomic_hormone_t hormone,
    autonomic_hormone_limits_t* limits
);

/**
 * @brief Set vital sign bounds
 *
 * WHAT: Configures bounds for a specific vital sign
 * WHY:  Different vitals have different normal ranges
 * HOW:  Stores bounds for monitoring and alerting
 *
 * @param gate Autonomic gate instance
 * @param vital Target vital sign
 * @param bounds Bounds to apply
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_set_vital_bounds(
    autonomic_gate_t* gate,
    autonomic_vital_t vital,
    const autonomic_vital_bounds_t* bounds
);

/**
 * @brief Get vital sign bounds
 *
 * WHAT: Retrieves current bounds for a vital sign
 * WHY:  Allows inspection of current bounds
 * HOW:  Copies current bounds to output structure
 *
 * @param gate Autonomic gate instance
 * @param vital Target vital sign
 * @param bounds Output buffer for bounds
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_get_vital_bounds(
    const autonomic_gate_t* gate,
    autonomic_vital_t vital,
    autonomic_vital_bounds_t* bounds
);

/**
 * @brief Get current hormone state
 *
 * WHAT: Retrieves current levels of all hormones
 * WHY:  Enables monitoring of internal state
 * HOW:  Copies current state to output structure
 *
 * @param gate Autonomic gate instance
 * @param state Output buffer for hormone state
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_get_hormone_state(
    const autonomic_gate_t* gate,
    autonomic_hormone_state_t* state
);

/**
 * @brief Read current vital signs
 *
 * WHAT: Retrieves current vital signs reading
 * WHY:  Provides snapshot of system health
 * HOW:  Copies current readings to output structure
 *
 * @param gate Autonomic gate instance
 * @param reading Output buffer for vitals reading
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_read_vitals(
    autonomic_gate_t* gate,
    autonomic_vitals_reading_t* reading
);

/**
 * @brief Update a vital sign value
 *
 * WHAT: Updates a vital sign (from external monitoring)
 * WHY:  Allows integration with external vital monitoring
 * HOW:  Validates change rate, updates value, triggers alerts
 *
 * @param gate Autonomic gate instance
 * @param vital Target vital sign
 * @param value New value for the vital sign
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_update_vital(
    autonomic_gate_t* gate,
    autonomic_vital_t vital,
    float value
);

/**
 * @brief Lock a hormone type
 *
 * WHAT: Prevents any changes to a specific hormone
 * WHY:  Selective locking for stability or testing
 * HOW:  Sets hormone lock flag, rejects all release requests
 *
 * @param gate Autonomic gate instance
 * @param hormone Hormone to lock
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_lock_hormone(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone
);

/**
 * @brief Unlock a hormone type
 *
 * WHAT: Re-enables changes to a specific hormone
 * WHY:  Restores operation after lock condition cleared
 * HOW:  Clears hormone lock flag
 *
 * @param gate Autonomic gate instance
 * @param hormone Hormone to unlock
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_unlock_hormone(
    autonomic_gate_t* gate,
    autonomic_hormone_t hormone
);

/**
 * @brief Trigger homeostasis correction
 *
 * WHAT: Manually triggers homeostasis correction cycle
 * WHY:  Forces return toward baseline levels
 * HOW:  Applies correction based on deviation from baseline
 *
 * @param gate Autonomic gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_trigger_homeostasis(autonomic_gate_t* gate);

/**
 * @brief Reset all hormones to baseline
 *
 * WHAT: Resets all hormone levels to their baseline values
 * WHY:  Emergency reset for unstable states
 * HOW:  Directly sets all levels to configured baselines
 *
 * @param gate Autonomic gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_reset_to_baseline(autonomic_gate_t* gate);

/**
 * @brief Get autonomic gate statistics
 *
 * WHAT: Retrieves operational statistics
 * WHY:  Enables monitoring and performance analysis
 * HOW:  Copies current statistics to output structure
 *
 * @param gate Autonomic gate instance
 * @param stats Output buffer for statistics
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_get_stats(
    const autonomic_gate_t* gate,
    autonomic_gate_stats_t* stats
);

/**
 * @brief Reset autonomic gate statistics
 *
 * WHAT: Clears all accumulated statistics
 * WHY:  Allows fresh statistics collection
 * HOW:  Zeros all statistic counters atomically
 *
 * @param gate Autonomic gate instance
 * @return NIMCP_SUCCESS or error code
 */
nimcp_result_t autonomic_gate_reset_stats(autonomic_gate_t* gate);

/**
 * @brief Get human-readable name for a hormone type
 *
 * @param hormone Hormone enum value
 * @return Static string name for the hormone
 */
const char* autonomic_hormone_name(autonomic_hormone_t hormone);

/**
 * @brief Get human-readable name for a vital sign
 *
 * @param vital Vital sign enum value
 * @return Static string name for the vital sign
 */
const char* autonomic_vital_name(autonomic_vital_t vital);

/**
 * @brief Get human-readable name for a release result
 *
 * @param result Release result enum value
 * @return Static string name for the result
 */
const char* autonomic_release_result_name(autonomic_release_result_t result);

/**
 * @brief Get human-readable name for a vital status
 *
 * @param status Vital status enum value
 * @return Static string name for the status
 */
const char* autonomic_vital_status_name(autonomic_vital_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LGSS_AUTONOMIC_GATE_H */
