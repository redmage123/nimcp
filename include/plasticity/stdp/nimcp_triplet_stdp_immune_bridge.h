/**
 * @file nimcp_triplet_stdp_immune_bridge.h
 * @brief Triplet STDP-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-19
 *
 * WHAT: Bidirectional integration between brain immune system and triplet STDP
 * WHY:  Inflammation affects triplet plasticity dynamics; abnormal triplet patterns
 *       indicate synaptic dysfunction requiring immune response
 * HOW:  Cytokines modulate triplet parameters (A2/A3 amplitudes, tau constants),
 *       excessive triplet LTP/LTD triggers immune surveillance
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → TRIPLET STDP PATHWAYS:
 * -------------------------------
 * 1. Cytokine Modulation of Triplet Terms:
 *    - Pro-inflammatory cytokines (IL-1β, IL-6, TNF-α) suppress triplet components
 *    - Triplet terms require metabolic resources (multi-spike integration)
 *    - Inflammation preferentially impairs A3 terms (energy conservation)
 *    - Reference: Pickering & O'Connor (2007) "Cytokines and synaptic plasticity"
 *
 * 2. Frequency-Dependent Effects:
 *    - Triplet STDP captures frequency dependence (>40 Hz → strong LTP)
 *    - Inflammation reduces high-frequency plasticity more than low-frequency
 *    - Models fever-induced impairment of burst-dependent learning
 *    - Pairwise (A2) less affected than triplet (A3) components
 *
 * 3. Trace Time Constant Modulation:
 *    - Chronic inflammation accelerates trace decay (reduced tau_x, tau_y)
 *    - Shortens temporal integration window
 *    - Impairs spike pattern recognition across longer timescales
 *    - Reference: Barrientos et al. (2009) "Inflammation and memory"
 *
 * 4. Learning Rate Suppression:
 *    - NONE: 100% learning (baseline)
 *    - LOCAL: 90% learning
 *    - REGIONAL: 70% learning
 *    - SYSTEMIC: 40% learning
 *    - STORM: 10% learning (critical suppression)
 *
 * TRIPLET STDP → IMMUNE PATHWAYS:
 * --------------------------------
 * 1. Runaway Triplet LTP Detection:
 *    - Excessive triplet accumulation without pairwise balance
 *    - Indicates hyperexcitability from high-frequency bursting
 *    - Triggers immune surveillance of synaptic homeostasis
 *    - Severity based on triplet/pairwise ratio
 *
 * 2. Slow Trace Dysfunction:
 *    - r2_pre, o2_post traces stuck or oscillating
 *    - Indicates temporal integration failure
 *    - Triggers moderate immune response
 *    - Models loss of sequence learning capacity
 *
 * 3. Frequency-Dependent Imbalance:
 *    - Normal: Low-frequency LTD, high-frequency LTP
 *    - Abnormal: All frequencies produce same plasticity
 *    - Loss of frequency selectivity → immune alert
 *    - Indicates disrupted metaplasticity
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              TRIPLET STDP-IMMUNE BRIDGE                                    ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → TRIPLET STDP                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   Cytokines → Triplet Parameter Modulation:                        │  ║
 * ║   │   ┌──────────────────────────────────────────────────┐             │  ║
 * ║   │   │ IL-1β: A3 → 60% (triplet suppression)           │             │  ║
 * ║   │   │ IL-6:  A2 → 75%, A3 → 50%                       │             │  ║
 * ║   │   │ TNF-α: tau_x, tau_y → 70% (faster decay)        │             │  ║
 * ║   │   │ IL-10: Restore baseline (recovery)              │             │  ║
 * ║   │   └──────────────────────────────────────────────────┘             │  ║
 * ║   │                                                                     │  ║
 * ║   │   Inflammation → Learning Rate:                                    │  ║
 * ║   │   ┌──────────────────────────────────────────────────┐             │  ║
 * ║   │   │ LOCAL:    90% (A2 × 0.9, A3 × 0.8)              │             │  ║
 * ║   │   │ REGIONAL: 70% (A2 × 0.7, A3 × 0.5)              │             │  ║
 * ║   │   │ SYSTEMIC: 40% (A2 × 0.4, A3 × 0.2)              │             │  ║
 * ║   │   │ STORM:    10% (A2 × 0.1, A3 × 0.05)             │             │  ║
 * ║   │   └──────────────────────────────────────────────────┘             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  TRIPLET STDP → IMMUNE                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   Abnormality Detection:                                           │  ║
 * ║   │   ┌──────────────────────────────────────────────────┐             │  ║
 * ║   │   │ Triplet/Pairwise Ratio > 5:  Runaway triplet    │             │  ║
 * ║   │   │ Slow trace variance > 3x:    Trace dysfunction  │             │  ║
 * ║   │   │ Frequency selectivity < 0.5: Metaplasticity loss│             │  ║
 * ║   │   └──────────────────────────────────────────────────┘             │  ║
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

#ifndef NIMCP_TRIPLET_STDP_IMMUNE_BRIDGE_H
#define NIMCP_TRIPLET_STDP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/stdp/nimcp_triplet_stdp.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine modulation of triplet parameters */
#define CYTOKINE_IL1_A2_IMPAIRMENT       0.70f   /**< IL-1β reduces A2 to 70% */
#define CYTOKINE_IL1_A3_IMPAIRMENT       0.60f   /**< IL-1β reduces A3 to 60% */
#define CYTOKINE_IL6_A2_IMPAIRMENT       0.75f   /**< IL-6 reduces A2 to 75% */
#define CYTOKINE_IL6_A3_IMPAIRMENT       0.50f   /**< IL-6 reduces A3 to 50% */
#define CYTOKINE_TNF_TAU_REDUCTION       0.70f   /**< TNF-α reduces tau to 70% */
#define CYTOKINE_IL10_RESTORATION        1.40f   /**< IL-10 restores by 40% */

/* Inflammation-based modulation by level */
#define INFLAMMATION_A2_NONE             1.00f   /**< 100% pairwise amplitude */
#define INFLAMMATION_A2_LOCAL            0.90f   /**< 90% pairwise */
#define INFLAMMATION_A2_REGIONAL         0.70f   /**< 70% pairwise */
#define INFLAMMATION_A2_SYSTEMIC         0.40f   /**< 40% pairwise */
#define INFLAMMATION_A2_STORM            0.10f   /**< 10% pairwise */

#define INFLAMMATION_A3_NONE             1.00f   /**< 100% triplet amplitude */
#define INFLAMMATION_A3_LOCAL            0.80f   /**< 80% triplet */
#define INFLAMMATION_A3_REGIONAL         0.50f   /**< 50% triplet */
#define INFLAMMATION_A3_SYSTEMIC         0.20f   /**< 20% triplet */
#define INFLAMMATION_A3_STORM            0.05f   /**< 5% triplet */

/* Abnormality detection thresholds */
#define TRIPLET_PAIRWISE_RATIO_THRESHOLD  5.0f   /**< Triplet/pairwise ratio */
#define SLOW_TRACE_VARIANCE_THRESHOLD     3.0f   /**< Variance multiplier */
#define FREQUENCY_SELECTIVITY_THRESHOLD   0.5f   /**< Selectivity score */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_THRESHOLD_SEC (86400.0f * 7)  /**< 7 days */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on triplet STDP parameters
 */
typedef struct {
    /* Pro-inflammatory effects on amplitudes */
    float il1_a2_impairment;        /**< IL-1β A2 reduction */
    float il1_a3_impairment;        /**< IL-1β A3 reduction */
    float il6_a2_impairment;        /**< IL-6 A2 reduction */
    float il6_a3_impairment;        /**< IL-6 A3 reduction */

    /* Pro-inflammatory effects on traces */
    float tnf_tau_reduction;        /**< TNF-α tau reduction */

    /* Anti-inflammatory effects */
    float il10_restoration;         /**< IL-10 restoration factor */

    /* Aggregate effects */
    float total_a2_modulation;      /**< Combined A2 scaling [0-2] */
    float total_a3_modulation;      /**< Combined A3 scaling [0-2] */
    float total_tau_modulation;     /**< Combined tau scaling [0-2] */
} cytokine_triplet_stdp_effects_t;

/**
 * @brief Inflammation effects on triplet STDP
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;
    bool is_chronic;

    /* Triplet STDP impacts */
    float a2_suppression;           /**< Pairwise amplitude reduction [0-1] */
    float a3_suppression;           /**< Triplet amplitude reduction [0-1] */
    float tau_acceleration;         /**< Trace decay acceleration [0-1] */
    float frequency_sensitivity_loss; /**< Loss of freq. dependence [0-1] */

    /* Chronic effects */
    float temporal_integration_loss; /**< Loss of sequence learning [0-1] */
    float burst_plasticity_loss;     /**< Loss of high-freq plasticity [0-1] */
} inflammation_triplet_stdp_state_t;

/**
 * @brief Triplet STDP abnormality detection
 */
typedef struct {
    /* Pairwise vs triplet balance */
    float total_ltp_pairwise;       /**< Recent pairwise LTP */
    float total_ltp_triplet;        /**< Recent triplet LTP */
    float total_ltd_pairwise;       /**< Recent pairwise LTD */
    float total_ltd_triplet;        /**< Recent triplet LTD */
    float triplet_pairwise_ratio;   /**< Triplet/pairwise ratio */

    /* Slow trace health */
    float r2_pre_mean;              /**< Average r2_pre */
    float r2_pre_variance;          /**< r2_pre variance */
    float o2_post_mean;             /**< Average o2_post */
    float o2_post_variance;         /**< o2_post variance */

    /* Frequency selectivity */
    float low_freq_plasticity;      /**< <10 Hz plasticity */
    float high_freq_plasticity;     /**< >40 Hz plasticity */
    float frequency_selectivity;    /**< high/low ratio */

    /* Abnormality flags */
    bool triplet_runaway_detected;  /**< Excessive triplet accumulation */
    bool slow_trace_dysfunction;    /**< r2/o2 trace instability */
    bool frequency_selectivity_loss; /**< Loss of freq. dependence */

    /* Severity */
    float instability_severity;     /**< Combined threat level [0-1] */
} triplet_stdp_instability_state_t;

/**
 * @brief Triplet STDP modulation snapshot
 */
typedef struct {
    /* Current modulation factors */
    float a2_plus_modulation;       /**< A2+ multiplier */
    float a2_minus_modulation;      /**< A2- multiplier */
    float a3_plus_modulation;       /**< A3+ multiplier */
    float a3_minus_modulation;      /**< A3- multiplier */
    float tau_plus_modulation;      /**< tau_plus multiplier */
    float tau_minus_modulation;     /**< tau_minus multiplier */
    float tau_x_modulation;         /**< tau_x multiplier */
    float tau_y_modulation;         /**< tau_y multiplier */

    /* Effective parameters (base * modulation) */
    float effective_A2_plus;
    float effective_A2_minus;
    float effective_A3_plus;
    float effective_A3_minus;
    float effective_tau_plus;
    float effective_tau_minus;
    float effective_tau_x;
    float effective_tau_y;
} triplet_stdp_modulation_state_t;

/**
 * @brief Complete triplet STDP-immune bridge state
 */
struct triplet_stdp_immune_bridge_struct {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* System handles */
    brain_immune_system_t* immune_system;
    triplet_stdp_synapse_t** synapses;  /**< Array of synapse pointers */
    size_t num_synapses;
    size_t synapse_capacity;

    /* Current state */
    cytokine_triplet_stdp_effects_t cytokine_effects;
    inflammation_triplet_stdp_state_t inflammation_state;
    triplet_stdp_instability_state_t instability_state;

    /* Base parameters (for restoration) */
    float base_A2_plus;
    float base_A2_minus;
    float base_A3_plus;
    float base_A3_minus;
    float base_tau_plus;
    float base_tau_minus;
    float base_tau_x;
    float base_tau_y;

    /* Integration flags */
    bool enable_cytokine_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_alerts;
    uint32_t plasticity_restorations;

};

/* Pointer typedef matching forward declaration in nimcp_triplet_stdp.h */
typedef struct triplet_stdp_immune_bridge_struct* triplet_stdp_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_modulation;
    bool enable_inflammation_impairment;
    bool enable_instability_detection;

    /* Sensitivity tuning */
    float cytokine_sensitivity;
    float inflammation_sensitivity;
    float instability_sensitivity;

    /* Base triplet STDP parameters */
    float base_A2_plus;
    float base_A2_minus;
    float base_A3_plus;
    float base_A3_minus;
    float base_tau_plus;
    float base_tau_minus;
    float base_tau_x;
    float base_tau_y;

    /* Thresholds */
    float triplet_pairwise_ratio_threshold;
    float slow_trace_variance_threshold;
    float frequency_selectivity_threshold;
} triplet_stdp_immune_config_t;

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
int triplet_stdp_immune_default_config(triplet_stdp_immune_config_t* config);

/**
 * @brief Create triplet STDP-immune bridge
 *
 * WHAT: Initialize bidirectional triplet STDP-immune integration
 * WHY:  Enable realistic inflammation-triplet plasticity coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param synapses Array of triplet STDP synapses
 * @param num_synapses Number of synapses
 * @return New bridge or NULL on failure
 */
triplet_stdp_immune_bridge_t triplet_stdp_immune_bridge_create(
    const triplet_stdp_immune_config_t* config,
    brain_immune_system_t* immune_system,
    triplet_stdp_synapse_t** synapses,
    size_t num_synapses
);

/**
 * @brief Destroy triplet STDP-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void triplet_stdp_immune_bridge_destroy(triplet_stdp_immune_bridge_t bridge);

/* ============================================================================
 * Immune → Triplet STDP API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to triplet STDP parameters
 *
 * WHAT: Modulate triplet STDP based on cytokine levels
 * WHY:  Pro-inflammatory cytokines impair triplet plasticity
 * HOW:  Query immune cytokines, adjust A2/A3/tau parameters
 *
 * @param bridge Triplet STDP-immune bridge
 * @return 0 on success
 */
int triplet_stdp_immune_apply_cytokine_effects(triplet_stdp_immune_bridge_t bridge);

/**
 * @brief Apply inflammation effects to triplet STDP
 *
 * WHAT: Reduce triplet learning from inflammation
 * WHY:  Chronic inflammation causes triplet plasticity deficits
 * HOW:  Check inflammation level/duration, suppress parameters
 *
 * @param bridge Triplet STDP-immune bridge
 * @return 0 on success
 */
int triplet_stdp_immune_apply_inflammation_effects(triplet_stdp_immune_bridge_t bridge);

/**
 * @brief Get modulation state for synapse
 *
 * WHAT: Compute current modulation factors
 * WHY:  Apply inflammation/cytokine effects to learning
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge Triplet STDP-immune bridge
 * @param modulation Output modulation state
 * @return 0 on success
 */
int triplet_stdp_immune_get_modulation_state(
    const triplet_stdp_immune_bridge_t bridge,
    triplet_stdp_modulation_state_t* modulation
);

/**
 * @brief Restore triplet STDP parameters after inflammation resolution
 *
 * WHAT: Return parameters to baseline
 * WHY:  IL-10 and resolution restore plasticity
 * HOW:  Interpolate back to base parameters
 *
 * @param bridge Triplet STDP-immune bridge
 * @param recovery_factor Recovery progress [0-1]
 * @return 0 on success
 */
int triplet_stdp_immune_restore_plasticity(
    triplet_stdp_immune_bridge_t bridge,
    float recovery_factor
);

/* ============================================================================
 * Triplet STDP → Immune API
 * ============================================================================ */

/**
 * @brief Detect triplet STDP instability
 *
 * WHAT: Check for unhealthy triplet plasticity patterns
 * WHY:  Abnormal triplet dynamics threaten homeostasis
 * HOW:  Monitor triplet/pairwise balance, trace health
 *
 * @param bridge Triplet STDP-immune bridge
 * @return 0 on success
 */
int triplet_stdp_immune_detect_instability(triplet_stdp_immune_bridge_t bridge);

/**
 * @brief Alert immune system of triplet instability
 *
 * WHAT: Notify immune system of triplet dysfunction
 * WHY:  Runaway triplet plasticity threatens neural integrity
 * HOW:  Create antigen from instability signature
 *
 * @param bridge Triplet STDP-immune bridge
 * @param antigen_id Output: created antigen ID
 * @return 0 on success
 */
int triplet_stdp_immune_alert_instability(
    triplet_stdp_immune_bridge_t bridge,
    uint32_t* antigen_id
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update triplet STDP-immune bridge (both directions)
 *
 * WHAT: Process all triplet STDP-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine/inflammation, detect instabilities
 *
 * @param bridge Triplet STDP-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int triplet_stdp_immune_bridge_update(
    triplet_stdp_immune_bridge_t bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Check if triplet plasticity is impaired by inflammation
 *
 * @param bridge Triplet STDP-immune bridge
 * @return true if impaired
 */
bool triplet_stdp_immune_is_plasticity_impaired(
    const triplet_stdp_immune_bridge_t bridge
);

/**
 * @brief Get triplet capacity reduction percentage
 *
 * @param bridge Triplet STDP-immune bridge
 * @return Reduction percentage [0-100]
 */
float triplet_stdp_immune_get_triplet_capacity_reduction(
    const triplet_stdp_immune_bridge_t bridge
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_TRIPLET_STDP
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int triplet_stdp_immune_connect_bio_async(triplet_stdp_immune_bridge_t bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int triplet_stdp_immune_disconnect_bio_async(triplet_stdp_immune_bridge_t bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool triplet_stdp_immune_is_bio_async_connected(
    const triplet_stdp_immune_bridge_t bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_TRIPLET_STDP_IMMUNE_BRIDGE_H */
