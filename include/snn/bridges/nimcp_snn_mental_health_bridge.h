/**
 * @file nimcp_snn_mental_health_bridge.h
 * @brief SNN-Mental Health integration bridge
 *
 * WHAT: Bidirectional bridge between SNN and mental health monitoring
 * WHY:  Enable spike-based dysregulation detection and intervention
 * HOW:  Detect abnormal spike patterns indicative of mental health states
 *
 * BIOLOGICAL BASIS:
 * - Depression: Reduced spike variability, low firing rates
 * - Anxiety: Excessive synchrony, high beta/gamma power
 * - Psychosis: Aberrant salience via dopaminergic dysregulation
 * - PTSD: Hyperactive amygdala, impaired extinction
 *
 * INTEGRATION:
 * - SNN → Mental Health: Detect dysregulation from spike patterns
 * - SNN → Mental Health: Identify risk markers (synchrony, variability)
 * - Mental Health → SNN: Interventions modulate network dynamics
 * - Mental Health → SNN: Normalize firing patterns
 *
 * @author NIMCP Team
 * @date 2024-12-20
 */

#ifndef NIMCP_SNN_MENTAL_HEALTH_BRIDGE_H
#define NIMCP_SNN_MENTAL_HEALTH_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "snn/nimcp_snn_types.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "snn/nimcp_snn_network.h"

//=============================================================================
// Configuration Types
//=============================================================================

/**
 * @brief Mental health risk level
 *
 * WHAT: Severity of detected dysregulation
 * WHY:  Guide intervention intensity
 * HOW:  Graduated levels based on deviation
 */
typedef enum {
    SNN_MENTAL_HEALTH_RISK_NONE = 0,    /**< No risk detected */
    SNN_MENTAL_HEALTH_RISK_LOW,         /**< Minor deviation */
    SNN_MENTAL_HEALTH_RISK_MODERATE,    /**< Moderate dysregulation */
    SNN_MENTAL_HEALTH_RISK_HIGH,        /**< Severe dysregulation */
    SNN_MENTAL_HEALTH_RISK_CRITICAL     /**< Critical state */
} snn_mental_health_risk_t;

/**
 * @brief Dysregulation type
 *
 * WHAT: Category of detected abnormality
 * WHY:  Different patterns indicate different conditions
 * HOW:  Pattern-based classification
 */
typedef enum {
    SNN_DYSREGULATION_NONE = 0,         /**< Normal function */
    SNN_DYSREGULATION_HYPOACTIVITY,     /**< Reduced activity (depression) */
    SNN_DYSREGULATION_HYPERACTIVITY,    /**< Excessive activity (anxiety) */
    SNN_DYSREGULATION_HYPERSYNCHRONY,   /**< Over-synchronized (epilepsy) */
    SNN_DYSREGULATION_DESYNCHRONY,      /**< Under-synchronized (psychosis) */
    SNN_DYSREGULATION_INSTABILITY       /**< Unstable dynamics (mood disorders) */
} snn_dysregulation_type_t;

/**
 * @brief SNN-Mental Health bridge configuration
 *
 * WHAT: Parameters for mental health monitoring
 * WHY:  Control dysregulation detection sensitivity
 * HOW:  Thresholds for each dysregulation type
 */
typedef struct snn_mental_health_config_s {
    /* Stability monitoring */
    float stability_threshold;          /**< Min stability index [0, 1] */
    float stability_window_ms;          /**< Window for stability computation */

    /* Risk detection */
    float risk_detection_sensitivity;   /**< Sensitivity [0, 1], higher = more sensitive */
    float hypoactivity_threshold;       /**< Min firing rate (Hz) */
    float hyperactivity_threshold;      /**< Max firing rate (Hz) */
    float hypersynchrony_threshold;     /**< Max synchrony [0, 1] */
    float desynchrony_threshold;        /**< Min synchrony [0, 1] */
    float instability_threshold;        /**< Max coefficient of variation */

    /* Intervention parameters */
    bool enable_auto_intervention;      /**< Auto-trigger interventions */
    float intervention_strength;        /**< Intervention modulation factor */
    float intervention_duration_ms;     /**< Duration of interventions */

    /* Population mapping */
    uint32_t monitor_population_id;     /**< Population to monitor */
    uint32_t limbic_population_id;      /**< Limbic system (emotion) */
    uint32_t prefrontal_population_id;  /**< Prefrontal cortex (regulation) */

    /* Update timing */
    float update_interval_ms;           /**< Monitoring update rate */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} snn_mental_health_config_t;

/**
 * @brief Mental health state
 *
 * WHAT: Current mental health metrics
 * WHY:  Track stability and risk
 * HOW:  Computed indices and counters
 */
typedef struct snn_mental_health_state_s {
    /* Stability metrics */
    float stability_index;              /**< Overall stability [0, 1] */
    float firing_rate;                  /**< Current firing rate (Hz) */
    float synchrony;                    /**< Current synchrony [0, 1] */
    float variability;                  /**< Coefficient of variation */

    /* Risk assessment */
    snn_mental_health_risk_t risk_level; /**< Current risk level */
    snn_dysregulation_type_t dysregulation_type; /**< Type of dysregulation */
    float risk_score;                   /**< Risk score [0, 1] */

    /* Intervention tracking */
    uint32_t intervention_count;        /**< Total interventions */
    bool intervention_active;           /**< Intervention currently active */
    float intervention_time_remaining;  /**< Time left in current intervention */

    /* Statistics */
    uint32_t update_count;              /**< Total updates */
    uint32_t dysregulation_detections;  /**< Total dysregulation events */
    float avg_stability;                /**< Average stability index */
} snn_mental_health_state_t;

/**
 * @brief SNN-Mental Health bridge structure
 *
 * WHAT: Context for SNN-mental health integration
 * WHY:  Maintain state of monitoring bridge
 * HOW:  Store references and cached state
 */
typedef struct snn_mental_health_bridge_s {
    bridge_base_t base;                 /**< MUST be first: base bridge infrastructure */

    snn_network_t* snn;                     /**< SNN network */
    snn_mental_health_config_t config;      /**< Bridge configuration */
    snn_mental_health_state_t state;        /**< Current state */

    /* Populations */
    snn_population_t* monitor_pop;          /**< Primary monitoring population */
    snn_population_t* limbic_pop;           /**< Limbic system */
    snn_population_t* prefrontal_pop;       /**< Prefrontal cortex */

    /* Timing */
    float last_update_time;                 /**< Last update timestamp (ms) */

    /* Bio-async */
    bool bio_async_enabled;                 /**< Bio-async connected */
    bio_module_context_t bio_ctx;           /**< Bio-async context */

    /* Mutex for thread safety */
    void* mutex;
} snn_mental_health_bridge_t;

//=============================================================================
// Default Configuration
//=============================================================================

/**
 * @brief Initialize mental health bridge config with defaults
 *
 * WHAT: Set biologically-plausible defaults
 * WHY:  Convenient initialization
 * HOW:  Values from clinical neuroscience literature
 *
 * @param config Config to initialize
 */
void snn_mental_health_config_default(snn_mental_health_config_t* config);

//=============================================================================
// Bridge Lifecycle
//=============================================================================

/**
 * @brief Create SNN-mental health bridge
 *
 * WHAT: Initialize monitoring bridge
 * WHY:  Enable mental health tracking
 * HOW:  Allocate context, set up monitoring
 *
 * @param config Bridge configuration
 * @param snn SNN network
 * @return Bridge instance or NULL on failure
 */
snn_mental_health_bridge_t* snn_mental_health_bridge_create(
    const snn_mental_health_config_t* config,
    snn_network_t* snn
);

/**
 * @brief Destroy SNN-mental health bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper cleanup
 * HOW:  Disconnect and free
 *
 * @param bridge Bridge to destroy
 */
void snn_mental_health_bridge_destroy(snn_mental_health_bridge_t* bridge);

/**
 * @brief Connect bridge to bio-async
 *
 * WHAT: Enable bio-async messaging
 * WHY:  Distributed health monitoring
 * HOW:  Register with bio-router
 *
 * @param bridge Bridge to connect
 * @return 0 on success, error code on failure
 */
int snn_mental_health_bridge_connect_bio_async(snn_mental_health_bridge_t* bridge);

/**
 * @brief Disconnect bridge from bio-async
 *
 * @param bridge Bridge to disconnect
 * @return 0 on success
 */
int snn_mental_health_bridge_disconnect_bio_async(snn_mental_health_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge to check
 * @return true if connected
 */
bool snn_mental_health_bridge_is_bio_async_connected(const snn_mental_health_bridge_t* bridge);

//=============================================================================
// Processing Functions
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Monitor mental health from spike patterns
 * WHY:  Detect dysregulation early
 * HOW:  Compute stability, assess risk, trigger interventions
 *
 * @param bridge Bridge to update
 * @param dt Time step in milliseconds
 * @return 0 on success, error code on failure
 */
int snn_mental_health_bridge_update(snn_mental_health_bridge_t* bridge, float dt);

//=============================================================================
// Dysregulation Detection
//=============================================================================

/**
 * @brief Compute stability index
 *
 * WHAT: Assess overall network stability
 * WHY:  Stability reflects mental health
 * HOW:  Combine firing rate, synchrony, variability metrics
 *
 * @param bridge Bridge instance
 * @return Stability index [0, 1]
 */
float snn_mental_health_compute_stability(snn_mental_health_bridge_t* bridge);

/**
 * @brief Detect dysregulation type
 *
 * WHAT: Classify abnormal spike patterns
 * WHY:  Different patterns indicate different conditions
 * HOW:  Compare metrics to thresholds
 *
 * @param bridge Bridge instance
 * @return Dysregulation type
 */
snn_dysregulation_type_t snn_mental_health_detect_dysregulation(
    snn_mental_health_bridge_t* bridge
);

/**
 * @brief Assess risk level
 *
 * WHAT: Determine severity of dysregulation
 * WHY:  Guide intervention intensity
 * HOW:  Map deviation magnitude to risk levels
 *
 * @param bridge Bridge instance
 * @return Risk level
 */
snn_mental_health_risk_t snn_mental_health_assess_risk(
    snn_mental_health_bridge_t* bridge
);

/**
 * @brief Compute risk score
 *
 * WHAT: Quantify mental health risk
 * WHY:  Continuous measure for monitoring
 * HOW:  Weighted combination of dysregulation metrics
 *
 * @param bridge Bridge instance
 * @return Risk score [0, 1]
 */
float snn_mental_health_compute_risk_score(snn_mental_health_bridge_t* bridge);

//=============================================================================
// Intervention Functions
//=============================================================================

/**
 * @brief Trigger intervention
 *
 * WHAT: Apply corrective modulation to network
 * WHY:  Normalize dysregulated dynamics
 * HOW:  Modulate population parameters based on dysregulation type
 *
 * @param bridge Bridge instance
 * @param dysregulation_type Type of dysregulation to address
 * @return 0 on success, error code on failure
 */
int snn_mental_health_trigger_intervention(
    snn_mental_health_bridge_t* bridge,
    snn_dysregulation_type_t dysregulation_type
);

/**
 * @brief Stop intervention
 *
 * WHAT: End current intervention
 * WHY:  Return to baseline after correction
 * HOW:  Reset modulation parameters
 *
 * @param bridge Bridge instance
 * @return 0 on success, error code on failure
 */
int snn_mental_health_stop_intervention(snn_mental_health_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get stability index
 *
 * @param bridge Bridge to query
 * @return Stability index [0, 1]
 */
float snn_mental_health_get_stability(const snn_mental_health_bridge_t* bridge);

/**
 * @brief Get risk level
 *
 * @param bridge Bridge to query
 * @return Current risk level
 */
snn_mental_health_risk_t snn_mental_health_get_risk_level(const snn_mental_health_bridge_t* bridge);

/**
 * @brief Get risk score
 *
 * @param bridge Bridge to query
 * @return Risk score [0, 1]
 */
float snn_mental_health_get_risk_score(const snn_mental_health_bridge_t* bridge);

/**
 * @brief Get dysregulation type
 *
 * @param bridge Bridge to query
 * @return Current dysregulation type
 */
snn_dysregulation_type_t snn_mental_health_get_dysregulation_type(
    const snn_mental_health_bridge_t* bridge
);

/**
 * @brief Check if intervention is active
 *
 * @param bridge Bridge to query
 * @return true if intervention active
 */
bool snn_mental_health_is_intervention_active(const snn_mental_health_bridge_t* bridge);

/**
 * @brief Get bridge state
 *
 * @param bridge Bridge to query
 * @param state Output state (copied)
 * @return 0 on success
 */
int snn_mental_health_bridge_get_state(
    const snn_mental_health_bridge_t* bridge,
    snn_mental_health_state_t* state
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Get intervention statistics
 *
 * @param bridge Bridge to query
 * @param intervention_count Output: total interventions
 * @param dysregulation_count Output: total dysregulation events
 * @param avg_stability Output: average stability index
 * @return 0 on success
 */
int snn_mental_health_get_stats(
    const snn_mental_health_bridge_t* bridge,
    uint32_t* intervention_count,
    uint32_t* dysregulation_count,
    float* avg_stability
);

/**
 * @brief Reset statistics
 *
 * @param bridge Bridge to reset
 */
void snn_mental_health_reset_stats(snn_mental_health_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SNN_MENTAL_HEALTH_BRIDGE_H */
