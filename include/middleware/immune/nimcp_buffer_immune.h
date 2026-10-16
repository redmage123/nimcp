//=============================================================================
// nimcp_buffer_immune.h - Buffer Immune System Integration
//=============================================================================

#ifndef NIMCP_BUFFER_IMMUNE_H
#define NIMCP_BUFFER_IMMUNE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/buffering/nimcp_circular_buffer.h"
#include "middleware/buffering/nimcp_integration_buffer.h"
#include "middleware/buffering/nimcp_phase_coded_buffer.h"
#include "middleware/buffering/nimcp_sliding_window.h"
#include "middleware/buffering/nimcp_temporal_accumulator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file nimcp_buffer_immune.h
 * @brief Immune system integration for middleware buffering modules
 *
 * WHAT: Bidirectional integration between brain immune system and buffers
 * WHY:  Neural buffer capacity is reduced during inflammation due to
 *       impaired synaptic function. Buffer corruption triggers immune alerts.
 * HOW:  Monitor inflammation levels to modulate buffer capacity. Detect
 *       buffer anomalies (overflow, corruption) and present as antigens.
 *
 * BIOLOGICAL BASIS:
 * ┌────────────────────────────────────────────────────────────────────┐
 * │ NEUROINFLAMMATION EFFECTS ON NEURAL BUFFERS                        │
 * ├────────────────────────────────────────────────────────────────────┤
 * │ 1. SYNAPTIC DYSFUNCTION                                            │
 * │    • Pro-inflammatory cytokines (IL-1β, TNF-α) impair LTP         │
 * │    • Reduced dendritic spine density                              │
 * │    • → Working memory capacity reduction (7±2 → 5±2)              │
 * │                                                                    │
 * │ 2. TEMPORAL INTEGRATION IMPAIRMENT                                │
 * │    • Inflammation reduces integration windows                     │
 * │    • Temporal accumulator decay accelerated                       │
 * │    • → Shorter effective time constants                           │
 * │                                                                    │
 * │ 3. PHASE COHERENCE DISRUPTION                                     │
 * │    • Inflammatory cytokines desynchronize theta oscillations      │
 * │    • → Phase-coded buffer coherence degradation                   │
 * │                                                                    │
 * │ 4. BUFFER OVERFLOW AS PATHOLOGY INDICATOR                         │
 * │    • Abnormal buffer pressure indicates processing dysfunction    │
 * │    • Persistent overflow → immune response trigger                │
 * │    • → Adaptive rebalancing via hierarchical recovery             │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * INTEGRATION MODES:
 * ┌────────────────────────────────────────────────────────────────────┐
 * │ IMMUNE → BUFFERS                                                   │
 * ├────────────────────────────────────────────────────────────────────┤
 * │ INFLAMMATION_NONE      → 100% capacity, normal integration        │
 * │ INFLAMMATION_LOCAL     →  90% capacity, 90% integration window    │
 * │ INFLAMMATION_REGIONAL  →  75% capacity, 75% integration window    │
 * │ INFLAMMATION_SYSTEMIC  →  50% capacity, 50% integration window    │
 * │ INFLAMMATION_STORM     →  25% capacity, 25% integration window    │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * ┌────────────────────────────────────────────────────────────────────┐
 * │ BUFFERS → IMMUNE                                                   │
 * ├────────────────────────────────────────────────────────────────────┤
 * │ Buffer overflow (>95% full)    → IL-1β release (alert)            │
 * │ Persistent overflow (>10 ops)  → TNF-α release (escalation)       │
 * │ Data corruption detected       → Antigen presentation (threat)    │
 * │ Coherence degradation (<0.3)   → IFN-γ release (quarantine)       │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * FEATURES:
 * - Inflammation-based capacity modulation
 * - Automatic immune alert on buffer anomalies
 * - Per-buffer health monitoring
 * - Cytokine-driven parameter adaptation
 * - Thread-safe immune callbacks
 */

//=============================================================================
// CONSTANTS
//=============================================================================

#define BUFFER_IMMUNE_MAX_BUFFERS     32    /**< Max monitored buffers */
#define BUFFER_IMMUNE_OVERFLOW_THRESH 0.95F /**< Overflow alert threshold */
#define BUFFER_IMMUNE_COHERENCE_MIN   0.3F  /**< Min coherence before alert */
#define BUFFER_IMMUNE_PERSISTENT_OVERFLOWS 10 /**< Overflows before escalation */

//=============================================================================
// TYPES
//=============================================================================

/**
 * @brief Buffer type identifier
 */
typedef enum {
    BUFFER_TYPE_CIRCULAR = 0,       /**< Circular buffer */
    BUFFER_TYPE_INTEGRATION,        /**< Multi-timescale integration buffer */
    BUFFER_TYPE_PHASE_CODED,        /**< Phase-coded working memory */
    BUFFER_TYPE_SLIDING_WINDOW,     /**< Sliding window buffer */
    BUFFER_TYPE_TEMPORAL_ACCUMULATOR /**< Temporal accumulator */
} buffer_type_t;

/**
 * @brief Buffer health status
 */
typedef enum {
    BUFFER_HEALTH_OPTIMAL = 0,      /**< Normal operation */
    BUFFER_HEALTH_STRESSED,         /**< High utilization */
    BUFFER_HEALTH_DEGRADED,         /**< Impaired by inflammation */
    BUFFER_HEALTH_CRITICAL,         /**< Near failure */
    BUFFER_HEALTH_FAILED            /**< Buffer non-functional */
} buffer_health_t;

/**
 * @brief Buffer anomaly types
 */
typedef enum {
    BUFFER_ANOMALY_NONE = 0,
    BUFFER_ANOMALY_OVERFLOW,        /**< Buffer overflow detected */
    BUFFER_ANOMALY_CORRUPTION,      /**< Data corruption */
    BUFFER_ANOMALY_COHERENCE_LOSS,  /**< Phase coherence degradation */
    BUFFER_ANOMALY_STARVATION,      /**< Chronic underutilization */
    BUFFER_ANOMALY_THRASHING        /**< Rapid fill/empty cycles */
} buffer_anomaly_t;

/**
 * @brief Buffer immune statistics
 */
typedef struct {
    uint64_t anomalies_detected;    /**< Total anomalies */
    uint64_t overflows_reported;    /**< Overflow events */
    uint64_t immune_alerts_sent;    /**< Immune alerts triggered */
    uint64_t capacity_reductions;   /**< Inflammation-driven reductions */
    float avg_health_score;         /**< Average health (0-1) */
    float current_modulation;       /**< Current capacity multiplier */
} buffer_immune_stats_t;

/**
 * @brief Buffer immune configuration
 */
typedef struct {
    bool enable_capacity_modulation;    /**< Modulate capacity on inflammation */
    bool enable_anomaly_detection;      /**< Detect and report anomalies */
    bool enable_auto_immune_alert;      /**< Auto-alert immune on anomaly */
    float overflow_threshold;           /**< Overflow detection threshold */
    float coherence_min_threshold;      /**< Min coherence threshold */
    uint32_t persistent_overflow_count; /**< Overflows before escalation */
    bool enable_logging;                /**< Enable debug logging */
} buffer_immune_config_t;

/**
 * @brief Registered buffer handle
 */
typedef struct {
    uint32_t id;                    /**< Buffer registration ID */
    buffer_type_t type;             /**< Buffer type */
    void* buffer_ptr;               /**< Buffer handle */
    char name[64];                  /**< Buffer name for logging */

    /* Health tracking */
    buffer_health_t health;         /**< Current health status */
    float health_score;             /**< Health score (0-1) */
    uint32_t consecutive_overflows; /**< Consecutive overflow count */
    uint64_t last_anomaly_time;     /**< Last anomaly timestamp */

    /* Capacity modulation state */
    size_t original_capacity;       /**< Original capacity */
    size_t modulated_capacity;      /**< Current effective capacity */
    float capacity_multiplier;      /**< Current multiplier (0-1) */

    /* Integration window modulation (for integration buffers) */
    float integration_window_multiplier; /**< Integration window scale */

    /* Anomaly tracking */
    buffer_anomaly_t last_anomaly;  /**< Last detected anomaly */
    uint32_t anomaly_count;         /**< Total anomalies */
} buffer_immune_handle_t;

/**
 * @brief Opaque buffer immune system
 */
typedef struct buffer_immune_system buffer_immune_system_t;

/**
 * @brief Callback for buffer anomaly detection
 */
typedef void (*buffer_immune_anomaly_cb_t)(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    buffer_anomaly_t anomaly,
    void* user_data
);

//=============================================================================
// LIFECYCLE API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with balanced parameters
 * HOW:  Return struct with recommended thresholds
 *
 * @param config Output configuration
 * @return 0 on success
 */
int buffer_immune_default_config(buffer_immune_config_t* config);

/**
 * @brief Create buffer immune system
 *
 * WHAT: Initialize buffer-immune integration
 * WHY:  Enable monitoring and modulation of buffers
 * HOW:  Allocate tracking structures, connect to brain immune
 *
 * @param brain_immune Brain immune system to integrate with
 * @param config Configuration (NULL for defaults)
 * @return New buffer immune system or NULL on failure
 */
buffer_immune_system_t* buffer_immune_create(
    brain_immune_system_t* brain_immune,
    const buffer_immune_config_t* config
);

/**
 * @brief Destroy buffer immune system
 *
 * WHAT: Clean up buffer immune resources
 * WHY:  Proper resource deallocation
 * HOW:  Free tracking structures, unregister buffers
 *
 * @param system System to destroy
 */
void buffer_immune_destroy(buffer_immune_system_t* system);

//=============================================================================
// BUFFER REGISTRATION API
//=============================================================================

/**
 * @brief Register circular buffer for immune monitoring
 *
 * WHAT: Add circular buffer to immune system
 * WHY:  Enable capacity modulation and anomaly detection
 * HOW:  Create handle, track original capacity
 *
 * @param system Buffer immune system
 * @param buffer Circular buffer to register
 * @param name Buffer name for logging
 * @param buffer_id Output: registration ID
 * @return 0 on success
 */
int buffer_immune_register_circular(
    buffer_immune_system_t* system,
    circular_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
);

/**
 * @brief Register integration buffer for immune monitoring
 *
 * WHAT: Add integration buffer to immune system
 * WHY:  Enable timescale window modulation on inflammation
 * HOW:  Create handle, track window sizes
 *
 * @param system Buffer immune system
 * @param buffer Integration buffer to register
 * @param name Buffer name for logging
 * @param buffer_id Output: registration ID
 * @return 0 on success
 */
int buffer_immune_register_integration(
    buffer_immune_system_t* system,
    integration_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
);

/**
 * @brief Register phase-coded buffer for immune monitoring
 *
 * WHAT: Add phase-coded buffer to immune system
 * WHY:  Monitor phase coherence degradation during inflammation
 * HOW:  Create handle, track coherence metrics
 *
 * @param system Buffer immune system
 * @param buffer Phase-coded buffer to register
 * @param name Buffer name for logging
 * @param buffer_id Output: registration ID
 * @return 0 on success
 */
int buffer_immune_register_phase_coded(
    buffer_immune_system_t* system,
    phase_coded_buffer_t* buffer,
    const char* name,
    uint32_t* buffer_id
);

/**
 * @brief Register sliding window for immune monitoring
 *
 * WHAT: Add sliding window to immune system
 * WHY:  Modulate window size on inflammation
 * HOW:  Create handle, track window parameters
 *
 * @param system Buffer immune system
 * @param window Sliding window to register
 * @param name Buffer name for logging
 * @param buffer_id Output: registration ID
 * @return 0 on success
 */
int buffer_immune_register_sliding_window(
    buffer_immune_system_t* system,
    sliding_window_t* window,
    const char* name,
    uint32_t* buffer_id
);

/**
 * @brief Register temporal accumulator for immune monitoring
 *
 * WHAT: Add temporal accumulator to immune system
 * WHY:  Modulate time constant (alpha) on inflammation
 * HOW:  Create handle, track integration parameters
 *
 * @param system Buffer immune system
 * @param accumulator Temporal accumulator to register
 * @param name Buffer name for logging
 * @param buffer_id Output: registration ID
 * @return 0 on success
 */
int buffer_immune_register_temporal_accumulator(
    buffer_immune_system_t* system,
    temporal_accumulator_t* accumulator,
    const char* name,
    uint32_t* buffer_id
);

/**
 * @brief Unregister buffer from immune monitoring
 *
 * WHAT: Remove buffer from immune system
 * WHY:  Buffer being destroyed or no longer needs monitoring
 * HOW:  Remove handle, restore original parameters
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer registration ID
 * @return 0 on success
 */
int buffer_immune_unregister(
    buffer_immune_system_t* system,
    uint32_t buffer_id
);

//=============================================================================
// IMMUNE → BUFFER MODULATION API
//=============================================================================

/**
 * @brief Apply inflammation-based capacity modulation
 *
 * WHAT: Reduce buffer capacity based on inflammation level
 * WHY:  Model synaptic dysfunction during neuroinflammation
 * HOW:  Scale capacity by inflammation-specific multiplier
 *
 * Capacity multipliers by inflammation level:
 * - NONE:      1.0 (100% capacity)
 * - LOCAL:     0.9 (90% capacity)
 * - REGIONAL:  0.75 (75% capacity)
 * - SYSTEMIC:  0.5 (50% capacity)
 * - STORM:     0.25 (25% capacity)
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer to modulate
 * @param inflammation_level Current inflammation level
 * @return 0 on success
 */
int buffer_immune_modulate_capacity(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Apply global inflammation modulation to all buffers
 *
 * WHAT: Modulate all registered buffers
 * WHY:  Systemic inflammation affects all neural buffers
 * HOW:  Apply modulation to each registered buffer
 *
 * @param system Buffer immune system
 * @param inflammation_level Current inflammation level
 * @return Number of buffers modulated
 */
int buffer_immune_modulate_all(
    buffer_immune_system_t* system,
    brain_inflammation_level_t inflammation_level
);

/**
 * @brief Restore buffer to original capacity
 *
 * WHAT: Remove inflammation-based modulation
 * WHY:  Inflammation resolved, restore normal function
 * HOW:  Reset capacity multiplier to 1.0
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer to restore
 * @return 0 on success
 */
int buffer_immune_restore_capacity(
    buffer_immune_system_t* system,
    uint32_t buffer_id
);

/**
 * @brief Restore all buffers to original capacity
 *
 * WHAT: Remove all inflammation modulation
 * WHY:  System-wide resolution
 * HOW:  Reset all multipliers to 1.0
 *
 * @param system Buffer immune system
 * @return Number of buffers restored
 */
int buffer_immune_restore_all(buffer_immune_system_t* system);

//=============================================================================
// BUFFER → IMMUNE ANOMALY DETECTION API
//=============================================================================

/**
 * @brief Report buffer overflow event
 *
 * WHAT: Notify immune system of buffer overflow
 * WHY:  Overflow indicates processing dysfunction
 * HOW:  Create antigen if persistent, release IL-1β cytokine
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer that overflowed
 * @param utilization Current utilization (0-1)
 * @return 0 on success
 */
int buffer_immune_report_overflow(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    float utilization
);

/**
 * @brief Report buffer corruption
 *
 * WHAT: Notify immune system of data corruption
 * WHY:  Corruption is high-severity threat
 * HOW:  Present as antigen immediately, release TNF-α
 *
 * @param system Buffer immune system
 * @param buffer_id Corrupted buffer
 * @param corruption_signature Corruption pattern
 * @param signature_len Signature length
 * @return 0 on success
 */
int buffer_immune_report_corruption(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    const uint8_t* corruption_signature,
    size_t signature_len
);

/**
 * @brief Report phase coherence degradation
 *
 * WHAT: Notify immune system of coherence loss
 * WHY:  Coherence loss indicates oscillatory dysfunction
 * HOW:  Present as antigen if below threshold, release IFN-γ
 *
 * @param system Buffer immune system
 * @param buffer_id Phase-coded buffer
 * @param coherence Current coherence (0-1)
 * @return 0 on success
 */
int buffer_immune_report_coherence_loss(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    float coherence
);

/**
 * @brief Check buffer health and report anomalies
 *
 * WHAT: Scan buffer state for anomalies
 * WHY:  Proactive anomaly detection
 * HOW:  Check utilization, coherence, thrashing patterns
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer to check
 * @param health Output: detected health status
 * @return 0 if healthy, 1 if anomaly detected
 */
int buffer_immune_check_health(
    buffer_immune_system_t* system,
    uint32_t buffer_id,
    buffer_health_t* health
);

/**
 * @brief Check all buffer health
 *
 * WHAT: Scan all registered buffers
 * WHY:  System-wide health check
 * HOW:  Check each buffer, aggregate results
 *
 * @param system Buffer immune system
 * @param worst_health Output: worst health detected
 * @return Number of unhealthy buffers
 */
int buffer_immune_check_all_health(
    buffer_immune_system_t* system,
    buffer_health_t* worst_health
);

//=============================================================================
// QUERY API
//=============================================================================

/**
 * @brief Get buffer health score
 *
 * WHAT: Retrieve normalized health score
 * WHY:  Quantify buffer status
 * HOW:  Combine utilization, anomaly rate, modulation
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer to query
 * @return Health score (0-1), 1.0 = optimal
 */
float buffer_immune_get_health_score(
    buffer_immune_system_t* system,
    uint32_t buffer_id
);

/**
 * @brief Get current capacity multiplier
 *
 * WHAT: Retrieve inflammation-based multiplier
 * WHY:  Check modulation status
 * HOW:  Return multiplier field
 *
 * @param system Buffer immune system
 * @param buffer_id Buffer to query
 * @return Capacity multiplier (0-1)
 */
float buffer_immune_get_capacity_multiplier(
    buffer_immune_system_t* system,
    uint32_t buffer_id
);

/**
 * @brief Get buffer immune statistics
 *
 * WHAT: Retrieve system-wide statistics
 * WHY:  Monitor integration health
 * HOW:  Aggregate stats from all buffers
 *
 * @param system Buffer immune system
 * @param stats Output statistics
 * @return 0 on success
 */
int buffer_immune_get_stats(
    buffer_immune_system_t* system,
    buffer_immune_stats_t* stats
);

/**
 * @brief Get registered buffer count
 *
 * WHAT: Number of currently registered buffers
 * WHY:  Check system load
 * HOW:  Return count field
 *
 * @param system Buffer immune system
 * @return Number of registered buffers
 */
uint32_t buffer_immune_get_buffer_count(
    buffer_immune_system_t* system
);

//=============================================================================
// CALLBACK API
//=============================================================================

/**
 * @brief Set anomaly detection callback
 *
 * WHAT: Register callback for buffer anomalies
 * WHY:  Custom anomaly handling
 * HOW:  Store callback and user data
 *
 * @param system Buffer immune system
 * @param callback Callback function
 * @param user_data User data pointer
 * @return 0 on success
 */
int buffer_immune_set_anomaly_callback(
    buffer_immune_system_t* system,
    buffer_immune_anomaly_cb_t callback,
    void* user_data
);

//=============================================================================
// UPDATE API
//=============================================================================

/**
 * @brief Update buffer immune system
 *
 * WHAT: Periodic health checks and modulation updates
 * WHY:  Maintain synchronized state with brain immune
 * HOW:  Poll inflammation levels, check buffer health
 *
 * @param system Buffer immune system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int buffer_immune_update(
    buffer_immune_system_t* system,
    uint64_t delta_ms
);

//=============================================================================
// UTILITY FUNCTIONS
//=============================================================================

/**
 * @brief Convert buffer type to string
 *
 * @param type Buffer type
 * @return String representation
 */
const char* buffer_immune_buffer_type_to_string(buffer_type_t type);

/**
 * @brief Convert health status to string
 *
 * @param health Health status
 * @return String representation
 */
const char* buffer_immune_health_to_string(buffer_health_t health);

/**
 * @brief Convert anomaly type to string
 *
 * @param anomaly Anomaly type
 * @return String representation
 */
const char* buffer_immune_anomaly_to_string(buffer_anomaly_t anomaly);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BUFFER_IMMUNE_H
