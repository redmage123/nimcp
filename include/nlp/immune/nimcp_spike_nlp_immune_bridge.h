/**
 * @file nimcp_spike_nlp_immune_bridge.h
 * @brief Spike-based NLP-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between brain immune system and spike-based NLP
 * WHY:  Inflammation affects spike timing and neural excitability; aberrant spike
 *       patterns signal neural distress. Essential for spike-level language modeling.
 * HOW:  Cytokines modulate spike rates and timing; spike train anomalies trigger
 *       immune responses; healthy spike patterns reduce inflammation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SPIKE NLP PATHWAYS:
 * ---------------------------
 * 1. Pro-inflammatory Cytokines:
 *    - Reduce neuronal excitability
 *    - Impair spike timing precision
 *    - Increase spike jitter
 *    - Reduce burst firing
 *    - Reference: Vezzani & Viviani (2015) "Neuroimmune interactions in inflammation
 *      and epilepsy: implications for novel therapies"
 *
 * 2. Chronic Inflammation:
 *    - Sustained reduction in spike rates
 *    - Loss of temporal coding precision
 *    - Degraded spike-timing-dependent plasticity
 *    - Reference: Stellwagen & Malenka (2006) "Synaptic scaling mediated by
 *      glial TNF-alpha"
 *
 * 3. Fever-Induced Effects:
 *    - Temperature sensitivity → spike rate changes
 *    - Increased spontaneous firing
 *    - Reduced signal-to-noise ratio
 *    - Reference: Kiyatkin (2005) "Brain hyperthermia as physiological and
 *      pathological phenomena"
 *
 * SPIKE NLP → IMMUNE PATHWAYS:
 * ---------------------------
 * 1. Aberrant Spike Patterns:
 *    - Excessive synchrony → seizure-like → immune activation
 *    - Spike bursts → stress response
 *    - Silence/quiescence → energy deficit → inflammation
 *    - Reference: Vezzani et al. (2011) "The role of inflammation in epilepsy"
 *
 * 2. Healthy Spike Dynamics:
 *    - Regular, decorrelated spikes → homeostatic signaling
 *    - Balanced E/I → anti-inflammatory
 *    - Efficient coding → reduced metabolic stress
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    SPIKE NLP-IMMUNE BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → SPIKE NLP PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-1β → -20% │  ───────┐                                       │  ║
 * ║   │   │ TNF-α → -30% │         │                                       │  ║
 * ║   │   │              │         ├──→ Spike Rate Reduction               │  ║
 * ║   │   └──────────────┘         │    Timing Jitter Increase             │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     SPIKE NLP SYSTEM            │                             │  ║
 * ║   │   │  - Reduced spike rates          │                             │  ║
 * ║   │   │  - Increased jitter             │                             │  ║
 * ║   │   │  - Impaired STDP                │                             │  ║
 * ║   │   │  - Degraded temporal codes      │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  SPIKE NLP → IMMUNE PATHWAYS                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ ABERRANT     │ ──→ Pro-inflammatory Cytokines                  │  ║
 * ║   │   │ SYNCHRONY    │ ──→ Immune Activation                           │  ║
 * ║   │   │ BURSTING     │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ HEALTHY      │ ──→ IL-10 Release                               │  ║
 * ║   │   │ DYNAMICS     │ ──→ Reduced Inflammation                        │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
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

#ifndef NIMCP_SPIKE_NLP_IMMUNE_BRIDGE_H
#define NIMCP_SPIKE_NLP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "nlp/nimcp_spike_nlp.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine spike modulation factors */
#define CYTOKINE_IL1_SPIKE_RATE_FACTOR    0.8f    /**< IL-1β → -20% spike rate */
#define CYTOKINE_IL6_SPIKE_RATE_FACTOR    0.85f   /**< IL-6 → -15% spike rate */
#define CYTOKINE_TNF_SPIKE_RATE_FACTOR    0.7f    /**< TNF-α → -30% spike rate */
#define CYTOKINE_IL10_SPIKE_RATE_FACTOR   1.1f    /**< IL-10 → +10% recovery */

/* Spike timing jitter from inflammation */
#define INFLAMMATION_JITTER_BASE          0.5f    /**< Base jitter (ms) */
#define INFLAMMATION_JITTER_PER_LEVEL     1.0f    /**< Jitter per level (ms) */

/* Spike pattern anomaly thresholds */
#define SPIKE_SYNCHRONY_THRESHOLD         0.7f    /**< Excessive synchrony */
#define SPIKE_BURST_THRESHOLD             5       /**< Spikes per burst */
#define SPIKE_SILENCE_THRESHOLD           100.0f  /**< Silence duration (ms) */
#define SPIKE_RATE_ANOMALY_THRESHOLD      2.0f    /**< Rate deviation (fold) */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine spike effects
 *
 * Represents how cytokines modulate spike generation
 */
typedef struct {
    /* Rate modulation */
    float il1_rate_factor;             /**< IL-1β rate modulation */
    float il6_rate_factor;             /**< IL-6 rate modulation */
    float tnf_rate_factor;             /**< TNF-α rate modulation */
    float il10_rate_factor;            /**< IL-10 rate modulation */

    /* Timing modulation */
    float timing_jitter_ms;            /**< Added timing jitter */
    float burst_probability;           /**< Probability of burst firing */
    float refractory_extension;        /**< Refractory period extension */

    /* Aggregate effects */
    float total_rate_modulation;       /**< Combined rate factor [0-2] */
    float spike_precision_loss;        /**< Timing precision loss [0-1] */
    float stdp_impairment;             /**< STDP effectiveness [0-1] */
} spike_cytokine_effects_t;

/**
 * @brief Inflammation spike state
 *
 * How inflammation affects spike dynamics
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;    /**< How long inflamed */

    /* Spike impacts */
    float rate_reduction;              /**< Rate reduction factor [0-1] */
    float timing_jitter_ms;            /**< Timing jitter (ms) */
    float excitability_change;         /**< Excitability change [-1,1] */
    float spontaneous_rate_increase;   /**< Spontaneous firing increase */

    /* Pattern degradation */
    float temporal_coding_loss;        /**< Temporal code degradation [0-1] */
    float burst_increase;              /**< Increased burst probability */
    float synchrony_increase;          /**< Increased synchrony [0-1] */
} spike_inflammation_state_t;

/**
 * @brief Spike-driven immune modulation
 *
 * How spike patterns affect immune function
 */
typedef struct {
    /* Spike pattern metrics */
    float current_spike_rate;          /**< Current firing rate (Hz) */
    float baseline_spike_rate;         /**< Expected baseline (Hz) */
    float synchrony_level;             /**< Population synchrony [0-1] */
    float burst_frequency;             /**< Burst events per second */
    float silence_duration_ms;         /**< Duration of silence */

    /* Anomaly detection */
    bool excessive_synchrony;          /**< Seizure-like activity */
    bool excessive_bursting;           /**< Pathological bursting */
    bool prolonged_silence;            /**< Energy deficit signal */
    bool rate_anomaly;                 /**< Rate significantly off */

    /* Immune effects */
    bool pattern_triggered_inflammation; /**< Anomaly triggered immune */
    float il10_from_healthy_dynamics;   /**< IL-10 from normal activity */

    /* Statistics */
    uint32_t total_spikes;             /**< Total spikes observed */
    uint32_t anomaly_events;           /**< Anomaly detections */
} spike_immune_modulation_t;

/**
 * @brief Spike NLP-immune bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_spike_modulation;
    bool enable_inflammation_jitter;
    bool enable_pattern_anomaly_detection;
    bool enable_healthy_dynamics_il10;
    bool enable_aberrant_pattern_inflammation;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float anomaly_sensitivity;         /**< Anomaly detection sensitivity [0.5-2.0] */

    /* Thresholds */
    float synchrony_threshold;         /**< Excessive synchrony [0.5-0.9] */
    uint32_t burst_threshold;          /**< Spikes per burst [3-10] */
    float silence_threshold_ms;        /**< Silence duration [50-200ms] */
    float rate_anomaly_threshold;      /**< Rate deviation [1.5-3.0] */
} spike_nlp_immune_config_t;

/**
 * @brief Complete spike NLP-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    neural_network_t network;

    /* Current state */
    spike_cytokine_effects_t cytokine_effects;
    spike_inflammation_state_t inflammation_state;
    spike_immune_modulation_t spike_modulation;

    /* Configuration */
    spike_nlp_immune_config_t config;

    /* Timing */
    uint64_t last_update_time;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t anomaly_triggers;
    uint32_t healthy_boosts;

    } spike_nlp_immune_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int spike_nlp_immune_default_config(spike_nlp_immune_config_t* config);

/**
 * @brief Create spike NLP-immune bridge
 *
 * WHAT: Initialize bidirectional spike NLP-immune integration
 * WHY:  Enable realistic immune-spike coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param network Neural network for spike NLP
 * @return New bridge or NULL on failure
 */
spike_nlp_immune_bridge_t* spike_nlp_immune_bridge_create(
    const spike_nlp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    neural_network_t network
);

/**
 * @brief Destroy spike NLP-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void spike_nlp_immune_bridge_destroy(spike_nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Spike NLP API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to spike generation
 *
 * WHAT: Modulate spike rates and timing based on cytokines
 * WHY:  Cytokines alter neuronal excitability
 * HOW:  Query immune cytokines, adjust spike thresholds/rates
 *
 * @param bridge Spike NLP-immune bridge
 * @return 0 on success
 */
int spike_nlp_immune_apply_cytokine_effects(spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Apply inflammation effects to spike timing
 *
 * WHAT: Add jitter and reduce precision from inflammation
 * WHY:  Inflammation degrades temporal coding
 * HOW:  Check inflammation level, add timing noise
 *
 * @param bridge Spike NLP-immune bridge
 * @return 0 on success
 */
int spike_nlp_immune_apply_inflammation_jitter(spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Compute spike rate modulation factor
 *
 * WHAT: Calculate overall spike rate scaling from immune state
 * WHY:  Inflammation reduces spike rates
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge Spike NLP-immune bridge
 * @return Rate modulation factor [0-2] (1.0 = normal)
 */
float spike_nlp_immune_compute_rate_modulation(const spike_nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Spike NLP → Immune API
 * ============================================================================ */

/**
 * @brief Detect and respond to spike pattern anomalies
 *
 * WHAT: Monitor spike patterns for pathological activity
 * WHY:  Aberrant patterns signal neural distress
 * HOW:  Compute synchrony, burst frequency, silence; trigger immune if anomalous
 *
 * @param bridge Spike NLP-immune bridge
 * @param spike_times Recent spike times (ms)
 * @param num_spikes Number of spikes
 * @param time_window_ms Time window for analysis
 * @return 0 on success
 */
int spike_nlp_immune_detect_pattern_anomalies(
    spike_nlp_immune_bridge_t* bridge,
    const uint64_t* spike_times,
    uint32_t num_spikes,
    float time_window_ms
);

/**
 * @brief Release IL-10 from healthy spike dynamics
 *
 * WHAT: Trigger anti-inflammatory response from normal spiking
 * WHY:  Healthy neural activity signals homeostasis
 * HOW:  Detect regular, decorrelated spikes; release IL-10
 *
 * @param bridge Spike NLP-immune bridge
 * @param spike_times Recent spike times
 * @param num_spikes Number of spikes
 * @return 0 on success
 */
int spike_nlp_immune_release_il10_from_healthy(
    spike_nlp_immune_bridge_t* bridge,
    const uint64_t* spike_times,
    uint32_t num_spikes
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update spike NLP-immune bridge (both directions)
 *
 * WHAT: Process all spike-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect anomalies, adjust parameters
 *
 * @param bridge Spike NLP-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int spike_nlp_immune_bridge_update(
    spike_nlp_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current spike rate modulation
 *
 * @param bridge Spike NLP-immune bridge
 * @return Rate modulation factor
 */
float spike_nlp_immune_get_rate_modulation(const spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Get current timing jitter
 *
 * @param bridge Spike NLP-immune bridge
 * @return Timing jitter in milliseconds
 */
float spike_nlp_immune_get_timing_jitter(const spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Check if spike patterns are anomalous
 *
 * @param bridge Spike NLP-immune bridge
 * @return true if anomalies detected
 */
bool spike_nlp_immune_has_pattern_anomaly(const spike_nlp_immune_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SPIKE_NLP
 *
 * @param bridge Spike NLP-immune bridge
 * @return 0 on success, -1 on error
 */
int spike_nlp_immune_connect_bio_async(spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Spike NLP-immune bridge
 * @return 0 on success
 */
int spike_nlp_immune_disconnect_bio_async(spike_nlp_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Spike NLP-immune bridge
 * @return true if connected
 */
bool spike_nlp_immune_is_bio_async_connected(const spike_nlp_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SPIKE_NLP_IMMUNE_BRIDGE_H */
