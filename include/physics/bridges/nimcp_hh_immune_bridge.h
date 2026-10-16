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
// nimcp_hh_immune_bridge.h - Hodgkin-Huxley to Immune System Bridge
//=============================================================================
/**
 * @file nimcp_hh_immune_bridge.h
 * @brief Bridge between HH biophysics and brain immune system
 *
 * WHAT: Bidirectional integration between Hodgkin-Huxley neuron dynamics and
 *       the brain immune system, enabling immune monitoring of neural health
 *       and immune modulation of neural function.
 *
 * WHY:  The immune system monitors and modulates neural function:
 *       - Channelopathies (ion channel dysfunction) indicate disease
 *       - Inflammation affects neural excitability
 *       - Microglia respond to aberrant neural activity
 *       - Cytokines modulate ion channel expression
 *       - Autoimmune conditions can target ion channels
 *
 * HOW:  - Reports HH channel states as health indicators
 *       - Detects channelopathies from aberrant dynamics
 *       - Receives cytokine signals affecting conductances
 *       - Monitors temperature/metabolic state
 *       - Interfaces with microglia for neuroprotection
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * NEURAL-IMMUNE INTERACTIONS:
 * ---------------------------
 * 1. Channel-Immune Communication:
 *    - Na+ channels: Anti-Nav antibodies in autoimmune disorders
 *    - K+ channels: Anti-VGKC complex antibodies (limbic encephalitis)
 *    - Ca2+ channels: Anti-VGCC (Lambert-Eaton syndrome)
 *    - Channelopathies trigger immune surveillance
 *
 * 2. Cytokine Effects on HH:
 *    - IL-1β: Increases neuronal excitability (seizures)
 *    - TNF-α: Modulates synaptic scaling
 *    - IL-6: Affects Ca2+ channel expression
 *    - IL-10: Neuroprotective, reduces excitotoxicity
 *
 * 3. Microglia-Neuron Interaction:
 *    - Microglia detect aberrant firing patterns
 *    - Synaptic pruning of dysfunctional synapses
 *    - Phagocytosis of damaged neurons
 *    - Neuroprotective factor release
 *
 * HH HEALTH INDICATORS:
 * ---------------------
 * 1. Channel Availability:
 *    - Reduced g_Na_max: Na+ channel antibodies or damage
 *    - Reduced g_K_max: K+ channelopathy
 *    - Altered kinetics: Temperature dysfunction
 *
 * 2. Firing Pattern Anomalies:
 *    - Hyperexcitability: Reduced threshold (pro-inflammatory)
 *    - Hypoexcitability: Increased threshold (anti-inflammatory)
 *    - Irregular patterns: Channel instability
 *    - Burst storms: Potential seizure activity
 *
 * 3. Metabolic Indicators:
 *    - Temperature deviations: Fever/hypothermia
 *    - ATP depletion: Metabolic stress
 *    - Ca2+ dysregulation: Excitotoxicity risk
 *
 * IMMUNE TO HH EFFECTS:
 * ---------------------
 * - Pro-inflammatory cytokines: Increase g_Na, reduce threshold
 * - Anti-inflammatory cytokines: Reduce excitability
 * - Microglia activation: May induce protective hyperpolarization
 * - Complement activation: Can damage ion channels
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 *
 * @author NIMCP Development Team
 * @date 2026-01-13
 * @version 1.0.0
 */

#ifndef NIMCP_HH_IMMUNE_BRIDGE_H
#define NIMCP_HH_IMMUNE_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "common/nimcp_export.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Module name for logging */
#define HH_IMMUNE_MODULE_NAME           "hh_immune_bridge"

/** Maximum monitored neurons */
#define HH_IMMUNE_MAX_NEURONS           1024

/** Health check interval (ms) */
#define HH_IMMUNE_HEALTH_INTERVAL       100.0f

/** Normal temperature range (Celsius) */
#define HH_IMMUNE_TEMP_NORMAL_MIN       36.0f
#define HH_IMMUNE_TEMP_NORMAL_MAX       38.0f

/** Fever threshold (Celsius) */
#define HH_IMMUNE_TEMP_FEVER            38.5f

/** Hypothermia threshold (Celsius) */
#define HH_IMMUNE_TEMP_HYPOTHERMIA      35.0f

/** Normal firing rate range (Hz) */
#define HH_IMMUNE_RATE_NORMAL_MIN       1.0f
#define HH_IMMUNE_RATE_NORMAL_MAX       100.0f

/** Seizure-like activity threshold (Hz) */
#define HH_IMMUNE_RATE_SEIZURE          200.0f

/** Channel health threshold */
#define HH_IMMUNE_CHANNEL_HEALTH_THRESH 0.5f

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neural health status
 */
typedef enum {
    HH_IMMUNE_STATUS_HEALTHY = 0,     /**< Normal function */
    HH_IMMUNE_STATUS_STRESSED,        /**< Under metabolic stress */
    HH_IMMUNE_STATUS_COMPROMISED,     /**< Function impaired */
    HH_IMMUNE_STATUS_CRITICAL,        /**< Severe dysfunction */
    HH_IMMUNE_STATUS_DAMAGED          /**< Irreversible damage */
} hh_immune_status_t;

/**
 * @brief Channelopathy type
 */
typedef enum {
    HH_CHANNELOPATHY_NONE = 0,        /**< No channelopathy */
    HH_CHANNELOPATHY_NA_REDUCED,      /**< Reduced Na+ function */
    HH_CHANNELOPATHY_NA_ENHANCED,     /**< Enhanced Na+ (hyperexcitable) */
    HH_CHANNELOPATHY_K_REDUCED,       /**< Reduced K+ function */
    HH_CHANNELOPATHY_K_ENHANCED,      /**< Enhanced K+ (hypoexcitable) */
    HH_CHANNELOPATHY_CA_DYSREGULATED, /**< Ca2+ dysregulation */
    HH_CHANNELOPATHY_MIXED            /**< Multiple channel issues */
} hh_channelopathy_t;

/**
 * @brief Inflammation level
 */
typedef enum {
    HH_INFLAMMATION_NONE = 0,         /**< No inflammation */
    HH_INFLAMMATION_LOCAL,            /**< Localized inflammation */
    HH_INFLAMMATION_REGIONAL,         /**< Regional spread */
    HH_INFLAMMATION_SYSTEMIC,         /**< Systemic inflammation */
    HH_INFLAMMATION_STORM             /**< Cytokine storm */
} hh_inflammation_level_t;

/**
 * @brief Immune response type
 */
typedef enum {
    HH_IMMUNE_RESPONSE_NONE = 0,      /**< No response needed */
    HH_IMMUNE_RESPONSE_MONITOR,       /**< Enhanced monitoring */
    HH_IMMUNE_RESPONSE_PROTECT,       /**< Neuroprotective measures */
    HH_IMMUNE_RESPONSE_INTERVENE,     /**< Active intervention */
    HH_IMMUNE_RESPONSE_QUARANTINE     /**< Isolate affected region */
} hh_immune_response_t;

/**
 * @brief Cytokine type affecting HH
 */
typedef enum {
    HH_CYTOKINE_IL1_BETA = 0,         /**< IL-1β (pro-inflammatory) */
    HH_CYTOKINE_IL6,                  /**< IL-6 (mixed effects) */
    HH_CYTOKINE_TNF_ALPHA,            /**< TNF-α (pro-inflammatory) */
    HH_CYTOKINE_IL10,                 /**< IL-10 (anti-inflammatory) */
    HH_CYTOKINE_IFN_GAMMA,            /**< IFN-γ (antiviral) */
    HH_CYTOKINE_COUNT
} hh_cytokine_t;

//=============================================================================
// Structures
//=============================================================================

/**
 * @brief HH health assessment
 */
typedef struct {
    uint32_t neuron_id;               /**< Neuron ID */
    hh_immune_status_t status;        /**< Overall health status */
    float health_score;               /**< Health score [0, 1] */

    /* Channel health */
    float na_channel_health;          /**< Na+ channel health [0, 1] */
    float k_channel_health;           /**< K+ channel health [0, 1] */
    float ca_channel_health;          /**< Ca2+ channel health [0, 1] */
    hh_channelopathy_t channelopathy; /**< Detected channelopathy */

    /* Activity metrics */
    float firing_rate_hz;             /**< Current firing rate */
    float rate_variability;           /**< Firing rate CV */
    bool hyperexcitable;              /**< Abnormally excitable */
    bool hypoexcitable;               /**< Abnormally quiet */
    bool seizure_like;                /**< Seizure-like activity */

    /* Metabolic state */
    float temperature;                /**< Current temperature (C) */
    float phi_factor;                 /**< Q10 factor */
    bool temperature_abnormal;        /**< Temperature out of range */
    float ca_concentration;           /**< Ca2+ level (uM) */
    bool ca_dysregulated;             /**< Ca2+ out of range */

    /* Timestamp */
    uint64_t assessment_time_ms;      /**< Assessment timestamp */
} hh_immune_health_t;

/**
 * @brief Cytokine signal affecting HH
 */
typedef struct {
    hh_cytokine_t type;               /**< Cytokine type */
    float concentration;              /**< Concentration [0, 1] */
    float g_na_effect;                /**< Effect on Na+ conductance */
    float g_k_effect;                 /**< Effect on K+ conductance */
    float threshold_effect;           /**< Effect on spike threshold */
    float duration_ms;                /**< Effect duration */
    uint64_t onset_time_ms;           /**< Effect onset time */
} hh_immune_cytokine_t;

/**
 * @brief Inflammation state affecting HH
 */
typedef struct {
    hh_inflammation_level_t level;    /**< Inflammation severity */
    float inflammation_score;         /**< Inflammation intensity [0, 1] */
    float local_temperature_rise;     /**< Temperature increase (C) */
    float excitability_change;        /**< Change in excitability */
    float edema_factor;               /**< Swelling effect */
    bool blood_brain_barrier_compromised; /**< BBB breach */
    uint32_t affected_neurons;        /**< Number affected */
} hh_immune_inflammation_t;

/**
 * @brief Immune response to HH state
 */
typedef struct {
    hh_immune_response_t response;    /**< Response type */
    uint32_t target_neuron;           /**< Target neuron (0 = population) */
    float response_strength;          /**< Response intensity [0, 1] */

    /* Specific interventions */
    float protective_hyperpolarization; /**< Protective V shift (mV) */
    float conductance_modulation;     /**< Conductance scaling */
    float anti_inflammatory_signal;   /**< Anti-inflammatory release */
    bool trigger_apoptosis;           /**< Trigger programmed death */

    /* Timing */
    float response_delay_ms;          /**< Response delay */
    float response_duration_ms;       /**< Response duration */
} hh_immune_action_t;

/**
 * @brief Population immune summary
 */
typedef struct {
    uint32_t population_size;         /**< Total neurons */
    uint32_t healthy_count;           /**< Healthy neurons */
    uint32_t stressed_count;          /**< Stressed neurons */
    uint32_t compromised_count;       /**< Compromised neurons */
    uint32_t critical_count;          /**< Critical neurons */
    uint32_t damaged_count;           /**< Damaged neurons */
    float mean_health_score;          /**< Population mean health */
    hh_inflammation_level_t inflammation; /**< Population inflammation */
    bool outbreak_detected;           /**< Spreading dysfunction */
} hh_immune_population_summary_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Health monitoring */
    float health_check_interval_ms;   /**< Health check frequency */
    float health_score_threshold;     /**< Threshold for concern */
    bool enable_continuous_monitoring; /**< Continuous vs periodic */

    /* Channel health thresholds */
    float na_health_threshold;        /**< Na+ health concern level */
    float k_health_threshold;         /**< K+ health concern level */
    float ca_health_threshold;        /**< Ca2+ health concern level */

    /* Activity thresholds */
    float hyperexcitable_rate_hz;     /**< Rate for hyperexcitability */
    float hypoexcitable_rate_hz;      /**< Rate for hypoexcitability */
    float seizure_threshold_hz;       /**< Rate for seizure-like */

    /* Temperature thresholds */
    float temp_normal_min;            /**< Normal temp minimum */
    float temp_normal_max;            /**< Normal temp maximum */
    float temp_fever_threshold;       /**< Fever threshold */
    float temp_hypothermia_threshold; /**< Hypothermia threshold */

    /* Cytokine effects */
    bool enable_cytokine_effects;     /**< Enable cytokine modulation */
    float cytokine_sensitivity;       /**< Cytokine effect scaling */

    /* Immune responses */
    bool enable_immune_responses;     /**< Enable active responses */
    float response_threshold;         /**< Health score for response */
    float quarantine_threshold;       /**< Health score for quarantine */

    /* Update parameters */
    float update_interval_ms;         /**< Bridge update interval */
} hh_immune_config_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    /* Health checks */
    uint64_t health_checks_performed; /**< Total health checks */
    uint64_t abnormalities_detected;  /**< Abnormalities found */
    uint64_t channelopathies_detected; /**< Channelopathies detected */

    /* Status distribution */
    uint64_t healthy_assessments;     /**< Healthy results */
    uint64_t stressed_assessments;    /**< Stressed results */
    uint64_t compromised_assessments; /**< Compromised results */
    uint64_t critical_assessments;    /**< Critical results */

    /* Cytokine effects */
    uint64_t cytokine_signals_processed; /**< Cytokine signals handled */
    float avg_cytokine_effect;        /**< Average cytokine effect */

    /* Immune responses */
    uint64_t immune_responses_triggered; /**< Responses triggered */
    uint64_t protective_interventions; /**< Protective measures */
    uint64_t quarantine_events;       /**< Quarantine events */

    /* Population */
    float avg_population_health;      /**< Average population health */
    float min_health_observed;        /**< Minimum health seen */
    uint64_t outbreak_events;         /**< Outbreak detections */

    /* Performance */
    float last_update_ms;             /**< Last update timestamp */
    float processing_latency_us;      /**< Processing latency */
} hh_immune_stats_t;

/** Opaque bridge handle */
typedef struct hh_immune_bridge_struct hh_immune_bridge_t;

//=============================================================================
// Configuration API
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Initialize configuration with clinical defaults
 * WHY:  Easy creation with medically-motivated thresholds
 * HOW:  Set normal ranges, response thresholds
 *
 * @param config Configuration to initialize
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_default_config(hh_immune_config_t* config);

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Create HH-immune bridge
 *
 * WHAT: Initialize bridge for neural health monitoring
 * WHY:  Enable immune surveillance of HH neurons
 * HOW:  Allocate monitoring state, initialize thresholds
 *
 * @param config Configuration (NULL for defaults)
 * @return Bridge handle or NULL on error
 */
NIMCP_EXPORT hh_immune_bridge_t* hh_immune_bridge_create(
    const hh_immune_config_t* config
);

/**
 * @brief Destroy bridge and free resources
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
NIMCP_EXPORT void hh_immune_bridge_destroy(hh_immune_bridge_t* bridge);

/**
 * @brief Reset bridge state
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_bridge_reset(hh_immune_bridge_t* bridge);

//=============================================================================
// Health Assessment API (HH to Immune)
//=============================================================================

/**
 * @brief Assess HH neuron health
 *
 * WHAT: Evaluate neuron health from HH state
 * WHY:  Detect dysfunction for immune response
 * HOW:  Check channels, rates, temperature, Ca2+
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to assess
 * @param voltage Current membrane voltage (mV)
 * @param g_na Current Na+ conductance fraction
 * @param g_k Current K+ conductance fraction
 * @param ca_i Intracellular Ca2+ (uM)
 * @param temperature Current temperature (C)
 * @param firing_rate Current firing rate (Hz)
 * @param health_out Output health assessment
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_assess_health(
    hh_immune_bridge_t* bridge,
    uint32_t neuron_id,
    float voltage,
    float g_na,
    float g_k,
    float ca_i,
    float temperature,
    float firing_rate,
    hh_immune_health_t* health_out
);

/**
 * @brief Detect channelopathy
 *
 * WHAT: Identify ion channel dysfunction
 * WHY:  Specific diagnosis for targeted response
 * HOW:  Analyze channel availability and kinetics
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to check
 * @param g_na_max Maximum Na+ conductance
 * @param g_k_max Maximum K+ conductance
 * @param tau_m_observed Observed m gate tau
 * @param tau_h_observed Observed h gate tau
 * @param channelopathy_out Output channelopathy type
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_detect_channelopathy(
    hh_immune_bridge_t* bridge,
    uint32_t neuron_id,
    float g_na_max,
    float g_k_max,
    float tau_m_observed,
    float tau_h_observed,
    hh_channelopathy_t* channelopathy_out
);

/**
 * @brief Get population health summary
 *
 * WHAT: Summarize health across population
 * WHY:  Detect outbreaks, population-level issues
 * HOW:  Aggregate individual assessments
 *
 * @param bridge Bridge handle
 * @param summary_out Output population summary
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_get_population_summary(
    const hh_immune_bridge_t* bridge,
    hh_immune_population_summary_t* summary_out
);

//=============================================================================
// Cytokine Effects API (Immune to HH)
//=============================================================================

/**
 * @brief Apply cytokine effect to HH
 *
 * WHAT: Modulate HH parameters by cytokine signal
 * WHY:  Cytokines affect neural excitability
 * HOW:  Adjust conductances and threshold
 *
 * @param bridge Bridge handle
 * @param cytokine Cytokine signal specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_apply_cytokine(
    hh_immune_bridge_t* bridge,
    const hh_immune_cytokine_t* cytokine
);

/**
 * @brief Get cumulative cytokine effects
 *
 * WHAT: Query total cytokine modulation
 * WHY:  Apply to HH parameters
 * HOW:  Sum all active cytokine effects
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to query
 * @param g_na_mod Output Na+ conductance modifier
 * @param g_k_mod Output K+ conductance modifier
 * @param threshold_mod Output threshold modifier (mV)
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_get_cytokine_effects(
    const hh_immune_bridge_t* bridge,
    uint32_t neuron_id,
    float* g_na_mod,
    float* g_k_mod,
    float* threshold_mod
);

/**
 * @brief Set inflammation level
 *
 * WHAT: Update inflammation state affecting HH
 * WHY:  Inflammation alters neural function
 * HOW:  Store level, compute effects
 *
 * @param bridge Bridge handle
 * @param inflammation Inflammation specification
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_set_inflammation(
    hh_immune_bridge_t* bridge,
    const hh_immune_inflammation_t* inflammation
);

/**
 * @brief Get current inflammation level
 *
 * @param bridge Bridge handle
 * @param inflammation_out Output inflammation state
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_get_inflammation(
    const hh_immune_bridge_t* bridge,
    hh_immune_inflammation_t* inflammation_out
);

//=============================================================================
// Immune Response API
//=============================================================================

/**
 * @brief Compute immune response to health state
 *
 * WHAT: Determine appropriate immune action
 * WHY:  Protect neural function
 * HOW:  Based on health assessment, select response
 *
 * @param bridge Bridge handle
 * @param health Current health assessment
 * @param action_out Output immune action
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_compute_response(
    hh_immune_bridge_t* bridge,
    const hh_immune_health_t* health,
    hh_immune_action_t* action_out
);

/**
 * @brief Apply immune response to HH
 *
 * WHAT: Execute immune action on HH neuron
 * WHY:  Implement protective/corrective measures
 * HOW:  Apply modulation, hyperpolarization, etc.
 *
 * @param bridge Bridge handle
 * @param action Immune action to apply
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_apply_response(
    hh_immune_bridge_t* bridge,
    const hh_immune_action_t* action
);

/**
 * @brief Trigger protective hyperpolarization
 *
 * WHAT: Emergency protection for critical neuron
 * WHY:  Prevent excitotoxic damage
 * HOW:  Inject hyperpolarizing current
 *
 * @param bridge Bridge handle
 * @param neuron_id Neuron to protect
 * @param strength Protection strength [0, 1]
 * @param duration_ms Protection duration
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_protect_neuron(
    hh_immune_bridge_t* bridge,
    uint32_t neuron_id,
    float strength,
    float duration_ms
);

/**
 * @brief Quarantine dysfunctional region
 *
 * WHAT: Isolate affected neurons from network
 * WHY:  Prevent spread of dysfunction
 * HOW:  Reduce connectivity, suppress activity
 *
 * @param bridge Bridge handle
 * @param neuron_ids Array of neurons to quarantine
 * @param count Number of neurons
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_quarantine(
    hh_immune_bridge_t* bridge,
    const uint32_t* neuron_ids,
    uint32_t count
);

//=============================================================================
// Update API
//=============================================================================

/**
 * @brief Update bridge state
 *
 * WHAT: Periodic bridge housekeeping
 * WHY:  Decay cytokines, check health, update responses
 * HOW:  Time-based state transitions
 *
 * @param bridge Bridge handle
 * @param dt_ms Time step in milliseconds
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_bridge_update(
    hh_immune_bridge_t* bridge,
    float dt_ms
);

//=============================================================================
// Statistics API
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_get_stats(
    const hh_immune_bridge_t* bridge,
    hh_immune_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * @param bridge Bridge handle
 * @return 0 on success, -1 on error
 */
NIMCP_EXPORT int hh_immune_reset_stats(hh_immune_bridge_t* bridge);

/**
 * @brief Get status name string
 *
 * @param status Health status
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_immune_status_name(hh_immune_status_t status);

/**
 * @brief Get channelopathy name string
 *
 * @param channelopathy Channelopathy type
 * @return Static string name
 */
NIMCP_EXPORT const char* hh_immune_channelopathy_name(hh_channelopathy_t channelopathy);

/**
 * @brief Print bridge summary
 *
 * @param bridge Bridge handle (NULL safe)
 */
NIMCP_EXPORT void hh_immune_print_summary(const hh_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HH_IMMUNE_BRIDGE_H */