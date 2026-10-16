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
// nimcp_thermo_hub_bridge.h - Thermodynamics to Cognitive Hub Bridge
//=============================================================================
/**
 * @file nimcp_thermo_hub_bridge.h
 * @brief Temperature modulation of cognitive hub coordination functions
 * @version 1.0.0
 * @date 2026-01-13
 *
 * WHAT: Bridges thermodynamics to the cognitive hub, modeling temperature
 *       effects on higher cognitive functions and cross-regional coordination.
 *
 * WHY:  Temperature affects cognitive performance and coordination:
 *       - Working memory capacity degrades outside optimal range
 *       - Attention and executive function are temperature-sensitive
 *       - Cross-regional synchronization depends on conduction velocities
 *       - Metabolic efficiency affects sustained cognitive effort
 *       - Fever impairs complex cognition before simple reflexes
 *       - Arousal and alertness depend on temperature homeostasis
 *
 * HOW:  - Monitors thermodynamic state (temperature, ATP, efficiency)
 *       - Modulates cognitive capacity and processing speed
 *       - Tracks coordination delays between brain regions
 *       - Models attention and executive function scaling
 *       - Integrates metabolic constraints on cognitive effort
 *       - Provides arousal and alertness modulation
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * TEMPERATURE EFFECTS ON COGNITION:
 * ---------------------------------
 * 1. Working Memory (Optimal: 36.5-37.5C):
 *    - Prefrontal cortex highly metabolically active
 *    - Sensitive to both hypo- and hyperthermia
 *    - Capacity degrades ~5% per degree outside optimal
 *    - Sustained attention requires thermal stability
 *
 * 2. Processing Speed (Q10 ~ 1.3):
 *    - Faster conduction at higher temperatures
 *    - But accuracy may decrease
 *    - Speed-accuracy tradeoff shifts with temperature
 *
 * 3. Executive Function (Optimal: 36.8-37.2C):
 *    - Narrowest optimal range
 *    - Most sensitive to temperature perturbation
 *    - Prefrontal circuits most vulnerable
 *
 * 4. Attention (Q10 ~ 1.2):
 *    - Alerting: arousal level, temperature-modulated
 *    - Orienting: spatial attention, moderately sensitive
 *    - Executive attention: highly temperature-sensitive
 *
 * CROSS-REGIONAL COORDINATION:
 * ----------------------------
 * 1. Synchronization:
 *    - Long-range coupling depends on conduction delays
 *    - Temperature affects axonal velocity
 *    - Phase relationships shift with temperature
 *
 * 2. Information Integration:
 *    - Binding requires precise timing
 *    - Temperature-induced jitter impairs binding
 *    - Consciousness correlates degrade at extremes
 *
 * 3. Default Mode Network:
 *    - Most metabolically active at rest
 *    - Very sensitive to ATP depletion
 *    - Temperature affects DMN connectivity
 *
 * METABOLIC CONSTRAINTS:
 * ----------------------
 * - Cognitive tasks increase metabolic demand 5-20%
 * - Prefrontal cortex has highest ATP turnover
 * - Sustained cognition depletes local ATP
 * - Temperature affects ATP production efficiency
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

#ifndef NIMCP_THERMO_HUB_BRIDGE_H
#define NIMCP_THERMO_HUB_BRIDGE_H

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
#define THERMO_HUB_MODULE_NAME              "thermo_hub_bridge"

/** Reference temperature (Kelvin) - body temperature */
#define THERMO_HUB_TEMP_REF_K               310.15f

/** Optimal cognitive temperature low bound (K) */
#define THERMO_HUB_OPTIMAL_LOW_K            309.65f

/** Optimal cognitive temperature high bound (K) */
#define THERMO_HUB_OPTIMAL_HIGH_K           310.65f

/** Q10 for processing speed */
#define THERMO_HUB_Q10_PROCESSING           1.3f

/** Q10 for attention systems */
#define THERMO_HUB_Q10_ATTENTION            1.2f

/** Q10 for coordination delay */
#define THERMO_HUB_Q10_COORDINATION         1.5f

/** Capacity degradation per degree K outside optimal */
#define THERMO_HUB_CAPACITY_DEGRADE_PER_K   0.05f

/** ATP threshold for full cognitive function */
#define THERMO_HUB_ATP_FULL                 0.8f

/** ATP threshold for degraded function */
#define THERMO_HUB_ATP_DEGRADED             0.5f

/** ATP threshold for minimal function */
#define THERMO_HUB_ATP_MINIMAL              0.3f

/** ATP cost per cognitive operation (moles) */
#define THERMO_HUB_ATP_PER_OPERATION        5.0e-17f

/** Default update interval (ms) */
#define THERMO_HUB_DEFAULT_UPDATE_MS        10.0f

/** Maximum coordination delay scaling */
#define THERMO_HUB_MAX_DELAY_FACTOR         3.0f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Cognitive function type
 */
typedef enum {
    THERMO_HUB_FUNC_WORKING_MEMORY = 0,     /**< Working memory */
    THERMO_HUB_FUNC_ATTENTION,              /**< Attention systems */
    THERMO_HUB_FUNC_EXECUTIVE,              /**< Executive function */
    THERMO_HUB_FUNC_PROCESSING_SPEED,       /**< Processing speed */
    THERMO_HUB_FUNC_MEMORY_ENCODING,        /**< Memory encoding */
    THERMO_HUB_FUNC_MEMORY_RETRIEVAL,       /**< Memory retrieval */
    THERMO_HUB_FUNC_LANGUAGE,               /**< Language processing */
    THERMO_HUB_FUNC_SPATIAL,                /**< Spatial cognition */
    THERMO_HUB_FUNC_COUNT
} thermo_hub_function_t;

/**
 * @brief Cognitive state/arousal level
 */
typedef enum {
    THERMO_HUB_STATE_UNCONSCIOUS = 0,       /**< Unconscious/coma */
    THERMO_HUB_STATE_DEEP_SLEEP,            /**< Deep sleep */
    THERMO_HUB_STATE_LIGHT_SLEEP,           /**< Light sleep */
    THERMO_HUB_STATE_DROWSY,                /**< Drowsy */
    THERMO_HUB_STATE_RELAXED,               /**< Awake, relaxed */
    THERMO_HUB_STATE_ALERT,                 /**< Alert, focused */
    THERMO_HUB_STATE_HYPERAROUSED           /**< Hyperaroused/stress */
} thermo_hub_state_t;

/**
 * @brief Brain region for coordination
 */
typedef enum {
    THERMO_HUB_REGION_PREFRONTAL = 0,       /**< Prefrontal cortex */
    THERMO_HUB_REGION_PARIETAL,             /**< Parietal cortex */
    THERMO_HUB_REGION_TEMPORAL,             /**< Temporal cortex */
    THERMO_HUB_REGION_OCCIPITAL,            /**< Occipital cortex */
    THERMO_HUB_REGION_HIPPOCAMPUS,          /**< Hippocampus */
    THERMO_HUB_REGION_THALAMUS,             /**< Thalamus */
    THERMO_HUB_REGION_CEREBELLUM,           /**< Cerebellum */
    THERMO_HUB_REGION_BRAINSTEM,            /**< Brainstem */
    THERMO_HUB_REGION_COUNT
} thermo_hub_region_t;

/**
 * @brief Cognitive performance level
 */
typedef enum {
    THERMO_HUB_PERF_OPTIMAL = 0,            /**< Optimal performance */
    THERMO_HUB_PERF_GOOD,                   /**< Good performance */
    THERMO_HUB_PERF_MODERATE,               /**< Moderate impairment */
    THERMO_HUB_PERF_POOR,                   /**< Poor performance */
    THERMO_HUB_PERF_CRITICAL                /**< Critical impairment */
} thermo_hub_performance_t;

//=============================================================================
// Configuration Structure
//=============================================================================

/**
 * @brief Configuration for thermo-hub bridge
 *
 * WHAT: All parameters controlling temperature effects on cognition
 * WHY:  Allows tuning cognitive temperature sensitivity
 * HOW:  Temperature ranges, Q10 values, ATP thresholds
 */
typedef struct {
    /* Temperature parameters */
    float reference_temp_k;                 /**< Reference temperature (K) */
    float optimal_low_k;                    /**< Optimal range low bound (K) */
    float optimal_high_k;                   /**< Optimal range high bound (K) */
    float critical_low_k;                   /**< Critical low temperature (K) */
    float critical_high_k;                  /**< Critical high temperature (K) */

    /* Q10 coefficients */
    float q10_processing;                   /**< Q10 for processing speed */
    float q10_attention;                    /**< Q10 for attention */
    float q10_coordination;                 /**< Q10 for coordination delays */

    /* Capacity degradation */
    float capacity_degrade_per_k;           /**< Capacity loss per degree K */
    float min_capacity;                     /**< Minimum capacity [0,1] */

    /* Function-specific sensitivities (relative to baseline) */
    float sensitivity_working_memory;       /**< Working memory sensitivity */
    float sensitivity_attention;            /**< Attention sensitivity */
    float sensitivity_executive;            /**< Executive function sensitivity */
    float sensitivity_processing;           /**< Processing speed sensitivity */
    float sensitivity_encoding;             /**< Memory encoding sensitivity */
    float sensitivity_retrieval;            /**< Memory retrieval sensitivity */

    /* Coordination parameters */
    float base_coordination_delay_ms;       /**< Base inter-region delay */
    float max_delay_factor;                 /**< Maximum delay scaling */
    float jitter_per_degree_k;              /**< Timing jitter per degree K */

    /* ATP parameters */
    float atp_full_threshold;               /**< ATP for full function [0,1] */
    float atp_degraded_threshold;           /**< ATP for degraded function */
    float atp_minimal_threshold;            /**< ATP for minimal function */
    float atp_per_operation;                /**< ATP per cognitive op (moles) */

    /* Arousal parameters */
    float base_arousal;                     /**< Baseline arousal [0,1] */
    float temp_arousal_sensitivity;         /**< Temperature effect on arousal */
    float arousal_decay_rate;               /**< Arousal decay rate */

    /* Feature flags */
    bool enable_capacity_modulation;        /**< Modulate cognitive capacity */
    bool enable_speed_modulation;           /**< Modulate processing speed */
    bool enable_coordination_delays;        /**< Model coordination delays */
    bool enable_arousal_tracking;           /**< Track arousal state */
    bool enable_atp_tracking;               /**< Track ATP consumption */
    bool enable_regional_effects;           /**< Per-region temperature effects */
    bool enable_thermal_protection;         /**< Protect at extreme temps */

    /* Update parameters */
    float update_interval_ms;               /**< Bridge update interval */
} thermo_hub_config_t;

//=============================================================================
// Cognitive Modulation Structure
//=============================================================================

/**
 * @brief Temperature-modulated cognitive parameters
 *
 * WHAT: Scaled cognitive parameters based on current temperature
 * WHY:  Provides ready-to-use parameters for cognitive simulation
 * HOW:  Temperature and ATP scaling applied
 */
typedef struct {
    /* Temperature state */
    float current_temp_k;                   /**< Current temperature (K) */
    float temp_deviation;                   /**< Deviation from optimal center */
    bool in_optimal_range;                  /**< Within optimal range */

    /* Cognitive capacity scaling */
    float overall_capacity;                 /**< Overall cognitive capacity [0,1] */
    float function_capacity[THERMO_HUB_FUNC_COUNT]; /**< Per-function capacity */

    /* Processing factors */
    float processing_speed_factor;          /**< Processing speed scaling */
    float attention_factor;                 /**< Attention system scaling */
    float accuracy_factor;                  /**< Accuracy scaling */
    float speed_accuracy_tradeoff;          /**< Current tradeoff position */

    /* Coordination */
    float coordination_delay_factor;        /**< Delay scaling factor */
    float coordination_jitter;              /**< Timing jitter (ms) */
    float synchronization_quality;          /**< Sync quality [0,1] */
    float inter_region_delay_ms[THERMO_HUB_REGION_COUNT]; /**< Per-region delays */

    /* Arousal and state */
    thermo_hub_state_t arousal_state;       /**< Current arousal state */
    float arousal_level;                    /**< Arousal level [0,1] */
    float alertness;                        /**< Alertness [0,1] */

    /* Performance assessment */
    thermo_hub_performance_t performance;   /**< Overall performance level */
    float performance_score;                /**< Numeric score [0,1] */

    /* ATP state */
    float atp_level;                        /**< Current ATP [0,1] */
    float atp_gate;                         /**< ATP gating factor [0,1] */
    float cognitive_reserve;                /**< Remaining capacity for effort */

    /* Protection state */
    bool thermal_warning;                   /**< Temperature warning active */
    float protection_factor;                /**< Activity reduction [0,1] */

    /* Timestamp */
    uint64_t last_update_us;                /**< Last update timestamp */
} thermo_hub_modulation_t;

//=============================================================================
// Statistics Structure
//=============================================================================

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Update counts */
    uint64_t updates_performed;             /**< Total bridge updates */
    uint64_t cognitive_operations;          /**< Cognitive operations tracked */

    /* Temperature stats */
    float min_temp_observed_k;              /**< Minimum temperature */
    float max_temp_observed_k;              /**< Maximum temperature */
    float avg_temp_k;                       /**< Average temperature */
    float time_in_optimal_pct;              /**< Percent time in optimal range */

    /* Performance stats */
    float avg_capacity;                     /**< Average cognitive capacity */
    float min_capacity;                     /**< Minimum capacity observed */
    float avg_performance_score;            /**< Average performance score */
    uint64_t time_optimal_us;               /**< Time at optimal performance */
    uint64_t time_impaired_us;              /**< Time at impaired performance */

    /* Per-function stats */
    float avg_function_capacity[THERMO_HUB_FUNC_COUNT];
    float min_function_capacity[THERMO_HUB_FUNC_COUNT];

    /* Processing stats */
    float avg_processing_factor;            /**< Average processing speed */
    float avg_accuracy_factor;              /**< Average accuracy */

    /* Coordination stats */
    float avg_coordination_delay;           /**< Average coordination delay */
    float avg_jitter;                       /**< Average timing jitter */
    float avg_sync_quality;                 /**< Average synchronization */

    /* Arousal stats */
    float avg_arousal;                      /**< Average arousal level */
    uint64_t state_time[7];                 /**< Time per arousal state (us) */
    uint64_t state_transitions;             /**< State transition count */

    /* ATP stats */
    double total_atp_consumed;              /**< Total ATP consumed (moles) */
    float avg_atp_level;                    /**< Average ATP level */
    uint64_t atp_limited_operations;        /**< Operations limited by ATP */

    /* Warning stats */
    uint64_t thermal_warnings;              /**< Thermal warning count */

    /* Timing */
    uint64_t start_time_us;                 /**< Bridge start time */
    uint64_t total_runtime_us;              /**< Total running time */
} thermo_hub_stats_t;

//=============================================================================
// Opaque Bridge Handle
//=============================================================================

/** Opaque bridge handle */
typedef struct thermo_hub_bridge_struct thermo_hub_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default configuration
 *
 * WHAT: Initialize configuration with cognitive science defaults
 * WHY:  Simplifies bridge creation
 * HOW:  Sets typical temperature sensitivities for cognition
 *
 * @param config    Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_default_config(thermo_hub_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create thermo-hub bridge
 *
 * WHAT: Allocate and initialize bridge instance
 * WHY:  Enables temperature modulation of cognitive functions
 * HOW:  Creates internal state, initializes tracking
 *
 * @param config    Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT thermo_hub_bridge_t* thermo_hub_bridge_create(
    const thermo_hub_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge    Bridge to destroy (NULL-safe)
 */
NIMCP_EXPORT void thermo_hub_bridge_destroy(thermo_hub_bridge_t* bridge);

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
NIMCP_EXPORT int thermo_hub_connect_thermo(
    thermo_hub_bridge_t* bridge,
    const nimcp_thermodynamic_state_t* thermo
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic update of cognitive modulation
 * WHY:  Recomputes scaling factors based on current state
 * HOW:  Reads temperature/ATP, updates cognitive parameters
 *
 * @param bridge    Bridge handle
 * @param dt_ms     Time step (milliseconds)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_update(
    thermo_hub_bridge_t* bridge,
    float dt_ms
);

/**
 * @brief Set temperature directly
 *
 * @param bridge        Bridge handle
 * @param temperature_k Temperature in Kelvin
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_set_temperature(
    thermo_hub_bridge_t* bridge,
    float temperature_k
);

/**
 * @brief Set ATP level directly
 *
 * @param bridge    Bridge handle
 * @param atp_level ATP level as fraction [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_set_atp(
    thermo_hub_bridge_t* bridge,
    float atp_level
);

/**
 * @brief Set arousal state
 *
 * @param bridge    Bridge handle
 * @param state     Target arousal state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_set_arousal(
    thermo_hub_bridge_t* bridge,
    thermo_hub_state_t state
);

/**
 * @brief Register cognitive operation for tracking
 *
 * WHAT: Record cognitive operation for ATP and statistics
 * WHY:  Enables metabolic cost accounting
 * HOW:  Deducts ATP, updates statistics
 *
 * @param bridge    Bridge handle
 * @param function  Cognitive function type
 * @param effort    Effort level [0,1]
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_register_operation(
    thermo_hub_bridge_t* bridge,
    thermo_hub_function_t function,
    float effort
);

/**
 * @brief Reset bridge state
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_reset(thermo_hub_bridge_t* bridge);

//=============================================================================
// Query API
//=============================================================================

/**
 * @brief Get current modulation parameters
 *
 * WHAT: Retrieve temperature-scaled cognitive parameters
 * WHY:  For applying modulation to cognitive simulation
 * HOW:  Copies current modulation state to output
 *
 * @param bridge        Bridge handle
 * @param modulation    Output modulation structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_get_modulation(
    const thermo_hub_bridge_t* bridge,
    thermo_hub_modulation_t* modulation
);

/**
 * @brief Get overall cognitive capacity
 *
 * @param bridge    Bridge handle
 * @return Current capacity [0,1]
 */
NIMCP_EXPORT float thermo_hub_get_capacity(
    const thermo_hub_bridge_t* bridge
);

/**
 * @brief Get capacity for specific function
 *
 * @param bridge    Bridge handle
 * @param function  Cognitive function
 * @return Function capacity [0,1]
 */
NIMCP_EXPORT float thermo_hub_get_function_capacity(
    const thermo_hub_bridge_t* bridge,
    thermo_hub_function_t function
);

/**
 * @brief Get processing speed factor
 *
 * @param bridge    Bridge handle
 * @return Processing speed factor
 */
NIMCP_EXPORT float thermo_hub_get_processing_speed(
    const thermo_hub_bridge_t* bridge
);

/**
 * @brief Get coordination delay for region
 *
 * @param bridge    Bridge handle
 * @param region    Target region
 * @return Coordination delay (ms), or -1 on error
 */
NIMCP_EXPORT float thermo_hub_get_coordination_delay(
    const thermo_hub_bridge_t* bridge,
    thermo_hub_region_t region
);

/**
 * @brief Get current arousal state
 *
 * @param bridge    Bridge handle
 * @return Current arousal state
 */
NIMCP_EXPORT thermo_hub_state_t thermo_hub_get_arousal_state(
    const thermo_hub_bridge_t* bridge
);

/**
 * @brief Get performance level
 *
 * @param bridge    Bridge handle
 * @return Current performance level
 */
NIMCP_EXPORT thermo_hub_performance_t thermo_hub_get_performance(
    const thermo_hub_bridge_t* bridge
);

/**
 * @brief Check if within optimal range
 *
 * @param bridge    Bridge handle
 * @return true if temperature is in optimal cognitive range
 */
NIMCP_EXPORT bool thermo_hub_is_optimal(
    const thermo_hub_bridge_t* bridge
);

/**
 * @brief Check if cognitive function is permitted
 *
 * WHAT: Check if conditions allow cognitive function
 * WHY:  Quick check before attempting operation
 * HOW:  Verifies temperature and ATP levels
 *
 * @param bridge    Bridge handle
 * @param function  Function to check
 * @param effort    Effort level required
 * @return true if function is permitted
 */
NIMCP_EXPORT bool thermo_hub_is_function_permitted(
    const thermo_hub_bridge_t* bridge,
    thermo_hub_function_t function,
    float effort
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge    Bridge handle
 * @param stats     Output statistics structure
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_get_stats(
    const thermo_hub_bridge_t* bridge,
    thermo_hub_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * @param bridge    Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int thermo_hub_reset_stats(thermo_hub_bridge_t* bridge);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Get function name
 *
 * @param function  Cognitive function
 * @return Function name string
 */
NIMCP_EXPORT const char* thermo_hub_function_name(
    thermo_hub_function_t function
);

/**
 * @brief Get arousal state name
 *
 * @param state Arousal state
 * @return State name string
 */
NIMCP_EXPORT const char* thermo_hub_state_name(thermo_hub_state_t state);

/**
 * @brief Get region name
 *
 * @param region    Brain region
 * @return Region name string
 */
NIMCP_EXPORT const char* thermo_hub_region_name(thermo_hub_region_t region);

/**
 * @brief Get performance level name
 *
 * @param perf  Performance level
 * @return Performance name string
 */
NIMCP_EXPORT const char* thermo_hub_performance_name(
    thermo_hub_performance_t perf
);

/**
 * @brief Print bridge summary to stdout
 *
 * @param bridge    Bridge handle
 */
NIMCP_EXPORT void thermo_hub_print_summary(const thermo_hub_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_THERMO_HUB_BRIDGE_H */