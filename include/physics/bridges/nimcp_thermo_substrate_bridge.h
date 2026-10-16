/* ============================================================================
 * [TOMBSTONE] DEPRECATED — proposed design, never implemented.
 *
 * This header declares a bridge API whose .c implementation was never written.
 * Any code that #includes this file and calls its functions will fail at link.
 * Preserved as a design record only; do NOT add new uses.
 *
 * Status: FULL-STATUE in the 2026-04-24 consumer-bridge audit. Ghost-typedef
 * bridges like this describe cross-module couplings that were sketched but
 * never implemented.
 *
 * To revive: write the backing .c file, add it to the appropriate CMakeLists,
 * then remove this banner and validate with the `_update`/`_create` caller
 * chain ending somewhere in a hot path. See
 *   docs/claude/consumer-bridge-inventory-2026-04-24.md
 * for the full inventory + the middle-path rationale for why this is
 * tombstoned rather than deleted or implemented.
 * ========================================================================= */

//=============================================================================
// nimcp_thermo_substrate_bridge.h - Thermodynamics to Substrate/Bio-Async Bridge
//=============================================================================
/**
 * @file nimcp_thermo_substrate_bridge.h
 * @brief Temperature effects on bio-async messaging substrate
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges thermodynamics to the bio-async messaging substrate,
 *       modeling temperature effects on neural communication.
 *
 * WHY:  Temperature affects all aspects of neural signaling:
 *       - Axonal conduction velocity (Q10 ~ 1.5)
 *       - Synaptic vesicle release probability
 *       - Neurotransmitter diffusion rates
 *       - Receptor binding kinetics
 *       - Signal propagation delays
 *       - Message queue processing rates
 *
 * HOW:  - Monitors thermodynamic state (temperature, ATP)
 *       - Modulates message transmission delays
 *       - Scales signal propagation velocities
 *       - Adjusts queue processing rates
 *       - Tracks metabolic cost of communication
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * TEMPERATURE EFFECTS ON NEURAL COMMUNICATION:
 * --------------------------------------------
 * 1. Axonal Conduction (Q10 ~ 1.5):
 *    - Conduction velocity: v(T) = v_ref * Q10^((T-T_ref)/10)
 *    - ~3-4% increase per degree Celsius
 *    - Affects all message propagation delays
 *
 * 2. Synaptic Transmission (Q10 ~ 2.0-3.0):
 *    - Vesicle fusion probability increases
 *    - Faster neurotransmitter release
 *    - Reduced synaptic delay
 *
 * 3. Diffusion (Q10 ~ 1.3):
 *    - D(T) proportional to T/eta(T)
 *    - Neurotransmitter clearance rate
 *    - Affects signal duration
 *
 * 4. Receptor Kinetics (Q10 ~ 2.5):
 *    - Binding/unbinding rates
 *    - Channel gating kinetics
 *    - Signal transduction speed
 *
 * 5. Metabolic Support:
 *    - ATP required for vesicle recycling
 *    - Ion gradient maintenance
 *    - Active transport processes
 *
 * MESSAGE QUEUE TEMPERATURE EFFECTS:
 * ----------------------------------
 * - Higher temp -> faster processing (shorter delays)
 * - ATP depletion -> increased failures, longer delays
 * - Extreme temps -> message degradation
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_THERMO_SUBSTRATE_BRIDGE_H
#define NIMCP_THERMO_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"
#include "physics/thermodynamics/nimcp_thermodynamics.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Module Constants
//=============================================================================

/** Module name for logging */
#define THERMO_SUBSTRATE_MODULE_NAME        "thermo_substrate_bridge"

/** Reference temperature (Kelvin) - body temperature */
#define THERMO_SUBSTRATE_TEMP_REF_K         310.15f

/** Q10 for axonal conduction velocity */
#define THERMO_SUBSTRATE_Q10_CONDUCTION     1.5f

/** Q10 for synaptic transmission */
#define THERMO_SUBSTRATE_Q10_SYNAPTIC       2.5f

/** Q10 for diffusion rate */
#define THERMO_SUBSTRATE_Q10_DIFFUSION      1.3f

/** Q10 for receptor kinetics */
#define THERMO_SUBSTRATE_Q10_RECEPTOR       2.5f

/** Q10 for queue processing rate */
#define THERMO_SUBSTRATE_Q10_PROCESSING     2.0f

/** Q10 for message reliability */
#define THERMO_SUBSTRATE_Q10_RELIABILITY    1.2f

/** ATP threshold for normal transmission */
#define THERMO_SUBSTRATE_ATP_FULL           0.6f

/** ATP threshold for degraded transmission */
#define THERMO_SUBSTRATE_ATP_DEGRADED       0.3f

/** ATP cost per message transmission (moles) */
#define THERMO_SUBSTRATE_ATP_PER_MESSAGE    1.0e-17f

/** Default update interval (ms) */
#define THERMO_SUBSTRATE_DEFAULT_UPDATE_MS  5.0f

/** Maximum delay scaling factor */
#define THERMO_SUBSTRATE_MAX_DELAY_FACTOR   5.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Message channel type for temperature effects
 */
typedef enum {
    THERMO_SUBSTRATE_CHANNEL_FAST = 0,      /**< Fast/myelinated pathways */
    THERMO_SUBSTRATE_CHANNEL_MEDIUM,        /**< Medium speed pathways */
    THERMO_SUBSTRATE_CHANNEL_SLOW,          /**< Slow/unmyelinated pathways */
    THERMO_SUBSTRATE_CHANNEL_DIFFUSE,       /**< Volume transmission */
    THERMO_SUBSTRATE_CHANNEL_COUNT
} thermo_substrate_channel_t;

/**
 * @brief Transmission quality level
 */
typedef enum {
    THERMO_SUBSTRATE_QUALITY_OPTIMAL = 0,   /**< Optimal conditions */
    THERMO_SUBSTRATE_QUALITY_NORMAL,        /**< Normal operation */
    THERMO_SUBSTRATE_QUALITY_DEGRADED,      /**< Degraded (ATP low) */
    THERMO_SUBSTRATE_QUALITY_CRITICAL,      /**< Critical (near failure) */
    THERMO_SUBSTRATE_QUALITY_FAILED         /**< Transmission failed */
} thermo_substrate_quality_t;

/**
 * @brief Temperature regime
 */
typedef enum {
    THERMO_SUBSTRATE_REGIME_HYPOTHERMIC = 0, /**< Below normal range */
    THERMO_SUBSTRATE_REGIME_NORMAL,          /**< Normal physiological */
    THERMO_SUBSTRATE_REGIME_FEBRILE,         /**< Fever range */
    THERMO_SUBSTRATE_REGIME_HYPERTHERMIC     /**< Dangerously high */
} thermo_substrate_regime_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-substrate bridge
 *
 * WHAT: All parameters controlling temperature effects on messaging
 * WHY:  Allows tuning temperature sensitivity of communication
 * HOW:  Q10 values, ATP thresholds, channel parameters
 */
typedef struct {
    /* Reference values */
    float reference_temp_k;                 /**< Reference temperature (K) */

    /* Q10 coefficients */
    float q10_conduction;                   /**< Q10 for conduction velocity */
    float q10_synaptic;                     /**< Q10 for synaptic transmission */
    float q10_diffusion;                    /**< Q10 for diffusion */
    float q10_receptor;                     /**< Q10 for receptor kinetics */
    float q10_processing;                   /**< Q10 for queue processing */
    float q10_reliability;                  /**< Q10 for message reliability */

    /* Channel-specific base delays (ms) */
    float base_delay_fast_ms;               /**< Fast channel base delay */
    float base_delay_medium_ms;             /**< Medium channel base delay */
    float base_delay_slow_ms;               /**< Slow channel base delay */
    float base_delay_diffuse_ms;            /**< Diffuse channel base delay */

    /* ATP parameters */
    float atp_full_threshold;               /**< ATP for full function [0,1] */
    float atp_degraded_threshold;           /**< ATP for degraded function */
    float atp_per_message;                  /**< ATP cost per message (moles) */
    float atp_per_byte;                     /**< ATP cost per byte (moles) */

    /* Reliability parameters */
    float base_reliability;                 /**< Baseline reliability [0,1] */
    float min_reliability;                  /**< Minimum reliability */
    float failure_rate_atp_factor;          /**< ATP effect on failure rate */

    /* Temperature limits */
    float hypothermia_threshold_k;          /**< Start of hypothermia */
    float hyperthermia_threshold_k;         /**< Start of hyperthermia */
    float critical_low_temp_k;              /**< Critical low temperature */
    float critical_high_temp_k;             /**< Critical high temperature */

    /* Processing limits */
    float max_delay_factor;                 /**< Maximum delay multiplier */
    float min_delay_factor;                 /**< Minimum delay multiplier */
    float max_queue_backlog;                /**< Max queued messages */

    /* Feature flags */
    bool enable_delay_modulation;           /**< Modulate delays by temp */
    bool enable_reliability_modulation;     /**< Modulate reliability */
    bool enable_atp_tracking;               /**< Track ATP consumption */
    bool enable_quality_degradation;        /**< Enable quality levels */
    bool enable_thermal_protection;         /**< Protect at extreme temps */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_substrate_config_t;

//=============================================================================
// Substrate Modulation Structure
//=============================================================================

/**
 * @brief Temperature-modulated substrate parameters
 *
 * WHAT: Scaled messaging parameters based on current temperature
 * WHY:  Provides ready-to-use parameters for bio-async system
 * HOW:  Q10 scaling applied to reference values
 */
typedef struct {
    /* Temperature state */
    float current_temp_k;                   /**< Current temperature (K) */
    float temp_deviation;                   /**< Deviation from reference */
    thermo_substrate_regime_t regime;       /**< Current temperature regime */

    /* Velocity/delay factors */
    float conduction_factor;                /**< Conduction velocity factor */
    float synaptic_factor;                  /**< Synaptic speed factor */
    float diffusion_factor;                 /**< Diffusion rate factor */
    float receptor_factor;                  /**< Receptor kinetics factor */
    float processing_factor;                /**< Queue processing factor */

    /* Channel-specific delays (temperature-scaled) */
    float delay_fast_ms;                    /**< Scaled fast delay */
    float delay_medium_ms;                  /**< Scaled medium delay */
    float delay_slow_ms;                    /**< Scaled slow delay */
    float delay_diffuse_ms;                 /**< Scaled diffuse delay */

    /* Reliability state */
    float reliability_factor;               /**< Reliability scaling [0,1] */
    float effective_reliability;            /**< Combined reliability */
    float failure_probability;              /**< Per-message failure prob */

    /* ATP state */
    float atp_level;                        /**< Current ATP [0,1] */
    float atp_gate;                         /**< ATP gating factor [0,1] */
    float atp_consumed;                     /**< ATP consumed this period */

    /* Quality assessment */
    thermo_substrate_quality_t quality;     /**< Current quality level */
    float quality_score;                    /**< Numeric quality [0,1] */

    /* Protection state */
    bool thermal_protection_active;         /**< Protection engaged */
    float protection_factor;                /**< Activity reduction [0,1] */

    /* Queue state */
    uint32_t messages_pending;              /**< Messages in queue */
    uint32_t messages_failed;               /**< Failed messages */
    float avg_queue_delay_ms;               /**< Average queue delay */

    /* Timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
} thermo_substrate_modulation_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t messages_transmitted;          /**< Total messages sent */
    uint64_t messages_failed;               /**< Failed transmissions */
    uint64_t bytes_transmitted;             /**< Total bytes sent */

    /* Temperature stats */
    float min_temp_observed_k;              /**< Minimum temperature */
    float max_temp_observed_k;              /**< Maximum temperature */
    float avg_temp_k;                       /**< Average temperature */

    /* Delay stats */
    float min_delay_observed_ms;            /**< Minimum delay seen */
    float max_delay_observed_ms;            /**< Maximum delay seen */
    float avg_delay_ms;                     /**< Average delay */
    float total_delay_ms;                   /**< Cumulative delay */

    /* Reliability stats */
    float avg_reliability;                  /**< Average reliability */
    float min_reliability;                  /**< Minimum reliability */
    uint64_t reliability_failures;          /**< Reliability-caused failures */

    /* ATP stats */
    double total_atp_consumed;              /**< Total ATP consumed (moles) */
    float avg_atp_level;                    /**< Average ATP level */
    uint64_t atp_limited_events;            /**< Events limited by ATP */

    /* Quality stats */
    uint64_t time_optimal_us;               /**< Time in optimal quality */
    uint64_t time_normal_us;                /**< Time in normal quality */
    uint64_t time_degraded_us;              /**< Time in degraded quality */
    uint64_t time_critical_us;              /**< Time in critical quality */

    /* Per-channel stats */
    uint64_t channel_messages[THERMO_SUBSTRATE_CHANNEL_COUNT];
    float channel_avg_delay[THERMO_SUBSTRATE_CHANNEL_COUNT];

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_substrate_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_substrate_bridge_struct thermo_substrate_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with biologically-plausible defaults
 * WHY:  Simplifies bridge creation
 * HOW:  Sets Q10 values and typical neural delays
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_default_config(
    thermo_substrate_config_t* config
);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-substrate bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables temperature modulation of messaging
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_substrate_bridge_t* thermo_substrate_bridge_create(
    const thermo_substrate_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_substrate_bridge_destroy(
    thermo_substrate_bridge_t* bridge
);

//=============================================================================
// Connection API
//=============================================================================

/**
 * @brief Connect bridge to thermodynamic state
 *
 * WHAT: Link bridge to thermodynamics module
 * WHY:  Enables real-time temperature/ATP monitoring
 * HOW:  Stores reference to thermodynamic state
 *
 * @param bridge    Bridge handle
 * @param thermo    Thermodynamic state to monitor
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_connect_thermo(
    thermo_substrate_bridge_t* bridge,
    const nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of substrate modulation
 * WHY:  Recomputes scaling factors based on current state
 * HOW:  Reads temperature/ATP, applies Q10 scaling
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_update(
    thermo_substrate_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_set_temperature(
    thermo_substrate_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set ATP level directly
 *
 * @param bridge    Bridge handle
 * @param atp_level ATP level as fraction [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_set_atp(
    thermo_substrate_bridge_t* bridge,
    float atp_level
);

/**
 * @brief Register message transmission for tracking
 *
 * WHAT: Record message for ATP and statistics tracking
 * WHY:  Enables metabolic cost accounting
 * HOW:  Deducts ATP, updates statistics
 *
 * @param bridge    Bridge handle
 * @param channel   Channel used for transmission
 * @param bytes     Message size in bytes
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_register_message(
    thermo_substrate_bridge_t* bridge,
    thermo_substrate_channel_t channel,
    uint32_t bytes
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_reset(thermo_substrate_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current modulation parameters
 *
 * WHAT: Retrieve temperature-scaled substrate parameters
 * WHY:  For applying modulation to bio-async system
 * HOW:  Copies current modulation state to output
 *
 * @param bridge        Bridge handle
 * @param modulation    Output modulation structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_get_modulation(
    const thermo_substrate_bridge_t* bridge,
    thermo_substrate_modulation_t* modulation
);

/**
 * @brief Get delay for specific channel
 *
 * @param bridge    Bridge handle
 * @param channel   Channel type
 * @return Temperature-scaled delay (ms), or -1 on error
 */
NIMCP_EXPORT float thermo_substrate_get_delay(
    const thermo_substrate_bridge_t* bridge,
    thermo_substrate_channel_t channel
);

/**
 * @brief Get current reliability
 *
 * @param bridge    Bridge handle
 * @return Current reliability [0,1], or -1 on error
 */
NIMCP_EXPORT float thermo_substrate_get_reliability(
    const thermo_substrate_bridge_t* bridge
);

/**
 * @brief Get current transmission quality
 *
 * @param bridge    Bridge handle
 * @return Current quality level
 */
NIMCP_EXPORT thermo_substrate_quality_t thermo_substrate_get_quality(
    const thermo_substrate_bridge_t* bridge
);

/**
 * @brief Check if transmission is reliable
 *
 * WHAT: Test if transmission should succeed (random check)
 * WHY:  For probabilistic transmission modeling
 * HOW:  Compares random value to current reliability
 *
 * @param bridge    Bridge handle
 * @return true if transmission should succeed
 */
NIMCP_EXPORT bool thermo_substrate_check_reliability(
    thermo_substrate_bridge_t* bridge
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_get_stats(
    const thermo_substrate_bridge_t* bridge,
    thermo_substrate_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_substrate_reset_stats(
    thermo_substrate_bridge_t* bridge
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get channel name
 *
 * @param channel   Channel type
 * @return Channel name string
 */
NIMCP_EXPORT const char* thermo_substrate_channel_name(
    thermo_substrate_channel_t channel
);

/**
 * @brief Get quality name
 *
 * @param quality   Quality level
 * @return Quality name string
 */
NIMCP_EXPORT const char* thermo_substrate_quality_name(
    thermo_substrate_quality_t quality
);

/**
 * @brief Get regime name
 *
 * @param regime    Temperature regime
 * @return Regime name string
 */
NIMCP_EXPORT const char* thermo_substrate_regime_name(
    thermo_substrate_regime_t regime
);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_substrate_print_summary(
    const thermo_substrate_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_SUBSTRATE_BRIDGE_H */