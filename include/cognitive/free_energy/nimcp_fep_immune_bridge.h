/**
 * @file nimcp_fep_immune_bridge.h
 * @brief Free Energy Principle - Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and brain immune system
 * WHY:  Biological evidence shows inflammation impairs precision-weighting and learning;
 *       high prediction errors can trigger immune responses (sickness behavior). Essential
 *       for realistic FEP-immune modeling.
 * HOW:  Inflammation reduces precision and learning rates; high prediction errors trigger
 *       antigen presentation; immune memory mirrors generative model updates.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → FEP PATHWAYS:
 * ----------------------
 * 1. Sickness Behavior as Inference Modification:
 *    - Inflammation → reduced exploration (precision-weighted PE)
 *    - Cytokines impair learning (reduced model updating)
 *    - Energy conservation → lower learning rates
 *    - Reference: Dantzer et al. (2008) "From inflammation to sickness and depression:
 *      when the immune system subjugates the brain"
 *
 * 2. Cytokine Effects on Precision Weighting:
 *    - IL-6, TNF-α → reduced sensory precision
 *    - Impaired Bayesian inference
 *    - Decreased confidence in predictions
 *    - Reference: Borsini et al. (2015) "Interferon-alpha-induced sickness behavior:
 *      an update on the role of inflammation"
 *
 * 3. Inflammation-Induced Learning Impairment:
 *    - Fever → synaptic plasticity suppression
 *    - Pro-inflammatory cytokines block LTP
 *    - Reduced model parameter updates
 *    - Reference: Avital et al. (2003) "Impaired interleukin-1 signaling is associated
 *      with deficits in hippocampal memory processes"
 *
 * 4. Cytokine Storm Effects:
 *    - Severe inflammation → delirium
 *    - Breakdown of generative models
 *    - Loss of predictive coherence
 *    - Reference: Wilson et al. (2020) "Delirium"
 *
 * FEP → IMMUNE PATHWAYS:
 * ----------------------
 * 1. Prediction Errors as Danger Signals:
 *    - High PE → immune activation
 *    - Model violations → threat detection
 *    - Surprise → danger signal
 *    - Biological rationale: Unexpected inputs may indicate pathogens
 *
 * 2. Model Updating as Immune Memory:
 *    - Belief updates parallel immune learning
 *    - Both minimize surprise/threat recurrence
 *    - Pattern recognition in both systems
 *    - Reference: Tschopp & Schroder (2010) "NLRP3 inflammasome activation:
 *      The convergence of multiple signaling pathways"
 *
 * 3. Active Inference and Immune Coordination:
 *    - Policy selection affects immune state
 *    - Exploration vs exploitation mirrors immune vigilance
 *    - Action selection under inflammation constraints
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP-IMMUNE BRIDGE                                       ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  IMMUNE → FEP PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ IL-6  → -0.3 │  ───────┐                                       │  ║
 * ║   │   │ TNF-α → -0.4 │         │                                       │  ║
 * ║   │   │ IL-10 → +0.2 │         ├──→ Precision Reduction                │  ║
 * ║   │   │              │         │    Learning Impairment                │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │     FEP SYSTEM                  │                             │  ║
 * ║   │   │  - Precision ↓                  │                             │  ║
 * ║   │   │  - Learning rate ↓              │                             │  ║
 * ║   │   │  - Sickness behavior active     │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────────────┐                                     │  ║
 * ║   │   │   INFLAMMATION LEVEL     │                                     │  ║
 * ║   │   │ ──────────────────────── │                                     │  ║
 * ║   │   │ LOCAL    → -5% precision │                                     │  ║
 * ║   │   │ REGIONAL → -15% precision│                                     │  ║
 * ║   │   │ SYSTEMIC → -40% precision│                                     │  ║
 * ║   │   │ STORM    → -70% precision│                                     │  ║
 * ║   │   └──────────────────────────┘                                     │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → IMMUNE PATHWAYS                              │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ HIGH PRED ERROR  │ ──→ Antigen Presentation                    │  ║
 * ║   │   │ MODEL VIOLATION  │ ──→ Immune Activation                       │  ║
 * ║   │   │ SURPRISE > 10.0  │ ──→ Threat Detection                        │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ BELIEF UPDATES   │ ──→ Immune Memory Formation                 │  ║
 * ║   │   │ CONVERGENCE      │ ──→ IL-10 Release (resolution)              │  ║
 * ║   │   └──────────────────┘                                             │  ║
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

#ifndef NIMCP_FEP_IMMUNE_BRIDGE_H
#define NIMCP_FEP_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Prediction error immune response thresholds */
#define FEP_IMMUNE_PE_THRESHOLD_LOW       2.0f    /**< Minor threat */
#define FEP_IMMUNE_PE_THRESHOLD_MEDIUM    5.0f    /**< Moderate threat */
#define FEP_IMMUNE_PE_THRESHOLD_HIGH     10.0f    /**< Severe threat */
#define FEP_IMMUNE_PE_THRESHOLD_CRITICAL 20.0f    /**< Critical threat */

/* Cytokine precision impact factors */
#define CYTOKINE_IL6_PRECISION_IMPACT     -0.30f  /**< IL-6 → precision reduction */
#define CYTOKINE_TNF_PRECISION_IMPACT     -0.40f  /**< TNF-α → precision reduction */
#define CYTOKINE_IL1_PRECISION_IMPACT     -0.20f  /**< IL-1β → precision reduction */
#define CYTOKINE_IFN_GAMMA_PRECISION_IMPACT -0.15f /**< IFN-γ → mild reduction */
#define CYTOKINE_IL10_PRECISION_IMPACT     0.20f  /**< IL-10 → recovery boost */

/* Cytokine learning rate impact factors */
#define CYTOKINE_IL6_LR_IMPAIRMENT        -0.25f  /**< IL-6 → learning reduction */
#define CYTOKINE_TNF_LR_IMPAIRMENT        -0.35f  /**< TNF-α → learning reduction */
#define CYTOKINE_IL1_LR_IMPAIRMENT        -0.15f  /**< IL-1β → learning reduction */
#define CYTOKINE_IL10_LR_BOOST             0.15f  /**< IL-10 → recovery boost */

/* Inflammation precision reduction */
#define INFLAMMATION_NONE_PRECISION_FACTOR     1.00f  /**< No reduction */
#define INFLAMMATION_LOCAL_PRECISION_FACTOR    0.95f  /**< -5% precision */
#define INFLAMMATION_REGIONAL_PRECISION_FACTOR 0.85f  /**< -15% precision */
#define INFLAMMATION_SYSTEMIC_PRECISION_FACTOR 0.60f  /**< -40% precision */
#define INFLAMMATION_STORM_PRECISION_FACTOR    0.30f  /**< -70% precision (delirium) */

/* Inflammation learning rate reduction */
#define INFLAMMATION_NONE_LR_FACTOR     1.00f  /**< No reduction */
#define INFLAMMATION_LOCAL_LR_FACTOR    0.95f  /**< -5% learning */
#define INFLAMMATION_REGIONAL_LR_FACTOR 0.80f  /**< -20% learning */
#define INFLAMMATION_SYSTEMIC_LR_FACTOR 0.50f  /**< -50% learning (fever) */
#define INFLAMMATION_STORM_LR_FACTOR    0.10f  /**< -90% learning (emergency) */

/* Sickness behavior thresholds */
#define SICKNESS_BEHAVIOR_THRESHOLD       0.5f   /**< Inflammation → sickness */
#define IMMUNE_MEMORY_TRANSFER_THRESHOLD  0.7f   /**< Belief → memory strength */
#define RECOVERY_CONVERGENCE_THRESHOLD    0.01f  /**< Low PE → IL-10 release */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct fep_immune_bridge fep_immune_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for FEP-Immune bridge
 */
typedef struct {
    /* Thresholds */
    float prediction_error_threshold;      /**< PE → immune activation */
    float inflammation_precision_factor;   /**< Inflammation → precision reduction */
    float cytokine_learning_impairment;    /**< IL-6/TNF → learning reduction */
    float recovery_rate;                   /**< Resolution recovery rate */

    /* Feature enables */
    bool enable_sickness_behavior;         /**< Inflammation → behavior changes */
    bool enable_immune_memory_transfer;    /**< Belief updates → immune memory */
    bool enable_pe_immune_activation;      /**< High PE → immune response */
    bool enable_convergence_il10;          /**< Low PE → IL-10 release */

    /* Sensitivity factors */
    float cytokine_sensitivity;            /**< Cytokine effect scaling */
    float inflammation_sensitivity;        /**< Inflammation effect scaling */
    float pe_sensitivity;                  /**< PE immune trigger scaling */
} fep_immune_config_t;

/**
 * @brief Cytokine effects on FEP
 */
typedef struct {
    /* Precision impacts */
    float il6_precision_reduction;         /**< IL-6 precision impact */
    float tnf_precision_reduction;         /**< TNF-α precision impact */
    float il1_precision_reduction;         /**< IL-1β precision impact */
    float ifn_gamma_precision_reduction;   /**< IFN-γ precision impact */
    float il10_precision_boost;            /**< IL-10 recovery boost */

    /* Learning impacts */
    float il6_learning_impairment;         /**< IL-6 learning impact */
    float tnf_learning_impairment;         /**< TNF-α learning impact */
    float il1_learning_impairment;         /**< IL-1β learning impact */
    float il10_learning_boost;             /**< IL-10 recovery boost */

    /* Total effects */
    float total_precision_reduction;       /**< Combined precision effect */
    float total_learning_impairment;       /**< Combined learning effect */
} fep_cytokine_effects_t;

/**
 * @brief Current state of FEP-Immune interaction
 */
typedef struct {
    /* Current inflammation */
    brain_inflammation_level_t inflammation_level;
    float current_inflammation;            /**< Inflammation intensity [0-1] */

    /* Applied modifiers */
    float precision_reduction;             /**< Current precision modifier */
    float learning_impairment;             /**< Current learning modifier */

    /* Sickness behavior */
    bool sickness_behavior_active;         /**< Sickness behavior engaged */
    float sickness_intensity;              /**< Sickness intensity [0-1] */

    /* Prediction error immune activation */
    uint32_t prediction_failures_reported; /**< High PEs reported to immune */
    uint64_t last_pe_report_time;          /**< Last PE report timestamp */

    /* Recovery state */
    float recovery_progress;               /**< Recovery progress [0-1] */
    bool converged;                        /**< Beliefs converged */
} fep_immune_state_t;

/**
 * @brief Statistics for FEP-Immune bridge
 */
typedef struct {
    /* Immune activations */
    uint64_t immune_activations;           /**< Times immune activated by PE */
    uint64_t prediction_failures;          /**< High PE events */
    uint64_t model_violations;             /**< Surprise threshold violations */

    /* Effects applied */
    float total_precision_reduction;       /**< Cumulative precision reduction */
    float total_learning_impairment;       /**< Cumulative learning reduction */
    float avg_inflammation;                /**< Average inflammation level */

    /* Recovery events */
    uint64_t convergence_il10_releases;    /**< IL-10 releases on convergence */
    uint64_t memory_transfers;             /**< Belief → immune memory transfers */

    /* Performance */
    float avg_free_energy_under_inflammation; /**< Avg F during inflammation */
    float avg_prediction_error;            /**< Avg PE magnitude */
} fep_immune_stats_t;

/**
 * @brief FEP-Immune bridge state
 */
struct fep_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    fep_immune_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;              /**< FEP system */
    brain_immune_system_t* immune_system;  /**< Brain immune system */

    /* Current effects */
    fep_cytokine_effects_t cytokine_effects;
    fep_immune_state_t state;

    /* Statistics */
    fep_immune_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default FEP-Immune configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int fep_immune_bridge_default_config(fep_immune_config_t* config);

/**
 * @brief Create FEP-Immune bridge
 *
 * WHAT: Initialize FEP-Immune integration bridge
 * WHY:  Enable bidirectional FEP-immune interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
fep_immune_bridge_t* fep_immune_bridge_create(const fep_immune_config_t* config);

/**
 * @brief Destroy FEP-Immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void fep_immune_bridge_destroy(fep_immune_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring and modulation
 * HOW:  Store FEP system pointer
 *
 * @param bridge FEP-Immune bridge
 * @param fep FEP system
 * @return 0 on success
 */
int fep_immune_bridge_connect_fep(
    fep_immune_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect brain immune system
 *
 * WHAT: Link bridge to immune system
 * WHY:  Enable immune state monitoring and activation
 * HOW:  Store immune system pointer
 *
 * @param bridge FEP-Immune bridge
 * @param immune Brain immune system
 * @return 0 on success
 */
int fep_immune_bridge_connect_immune(
    fep_immune_bridge_t* bridge,
    brain_immune_system_t* immune
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and immune systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_bridge_disconnect(fep_immune_bridge_t* bridge);

/* ============================================================================
 * FEP → Immune Direction
 * ============================================================================ */

/**
 * @brief Report prediction failure to immune system
 *
 * WHAT: Trigger immune response for high prediction error
 * WHY:  Unexpected inputs may indicate threats
 * HOW:  Present PE pattern as antigen if above threshold
 *
 * @param bridge FEP-Immune bridge
 * @param magnitude Prediction error magnitude
 * @return 0 on success
 */
int fep_immune_report_prediction_failure(
    fep_immune_bridge_t* bridge,
    float magnitude
);

/**
 * @brief Report model violation to immune system
 *
 * WHAT: Trigger immune response for generative model violations
 * WHY:  Model violations represent unexpected patterns (threats)
 * HOW:  Present violation pattern as antigen
 *
 * @param bridge FEP-Immune bridge
 * @param pattern Violation pattern
 * @param len Pattern length
 * @return 0 on success
 */
int fep_immune_report_model_violation(
    fep_immune_bridge_t* bridge,
    const uint8_t* pattern,
    size_t len
);

/**
 * @brief Transfer belief updates to immune memory
 *
 * WHAT: Sync FEP belief updates to immune memory cells
 * WHY:  Both systems learn threat patterns
 * HOW:  Convert belief state to immune memory signature
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_transfer_belief_to_memory(fep_immune_bridge_t* bridge);

/**
 * @brief Release IL-10 on FEP convergence
 *
 * WHAT: Trigger anti-inflammatory response on belief convergence
 * WHY:  Low PE indicates threat resolved
 * HOW:  Release IL-10 cytokine when PE below threshold
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_convergence_il10_release(fep_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → FEP Direction
 * ============================================================================ */

/**
 * @brief Apply inflammation effects to FEP
 *
 * WHAT: Modify FEP precision and learning based on inflammation
 * WHY:  Sickness behavior reduces exploration and learning
 * HOW:  Scale precision/learning by inflammation level
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_apply_inflammation_effects(fep_immune_bridge_t* bridge);

/**
 * @brief Get precision modifier from immune state
 *
 * WHAT: Calculate current precision scaling factor
 * WHY:  Inflammation reduces confidence in predictions
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge FEP-Immune bridge
 * @param modifier Output precision modifier [0-1]
 * @return 0 on success
 */
int fep_immune_get_precision_modifier(
    const fep_immune_bridge_t* bridge,
    float* modifier
);

/**
 * @brief Get learning modifier from immune state
 *
 * WHAT: Calculate current learning rate scaling factor
 * WHY:  Fever/inflammation suppresses plasticity
 * HOW:  Combine cytokine and inflammation effects
 *
 * @param bridge FEP-Immune bridge
 * @param modifier Output learning modifier [0-1]
 * @return 0 on success
 */
int fep_immune_get_learning_modifier(
    const fep_immune_bridge_t* bridge,
    float* modifier
);

/**
 * @brief Update cytokine effects on FEP
 *
 * WHAT: Compute current cytokine impact on precision/learning
 * WHY:  Cytokines modulate cognitive function
 * HOW:  Query immune cytokine levels, compute effects
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_update_cytokine_effects(fep_immune_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update FEP-Immune bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep FEP and immune systems synchronized
 * HOW:  Update cytokine effects, check PE thresholds, apply modifiers
 *
 * @param bridge FEP-Immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int fep_immune_bridge_update(
    fep_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge FEP-Immune bridge
 * @param state Output state
 * @return 0 on success
 */
int fep_immune_bridge_get_state(
    const fep_immune_bridge_t* bridge,
    fep_immune_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge FEP-Immune bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int fep_immune_bridge_get_stats(
    const fep_immune_bridge_t* bridge,
    fep_immune_stats_t* stats
);

/**
 * @brief Check if sickness behavior is active
 *
 * @param bridge FEP-Immune bridge
 * @return true if sickness behavior engaged
 */
bool fep_immune_is_sickness_active(const fep_immune_bridge_t* bridge);

/**
 * @brief Get current inflammation level
 *
 * @param bridge FEP-Immune bridge
 * @return Current inflammation level
 */
brain_inflammation_level_t fep_immune_get_inflammation_level(
    const fep_immune_bridge_t* bridge
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for FEP-immune coordination
 * WHY:  Distributed immune signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_bridge_connect_bio_async(fep_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge FEP-Immune bridge
 * @return 0 on success
 */
int fep_immune_bridge_disconnect_bio_async(fep_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge FEP-Immune bridge
 * @return true if bio-async enabled
 */
bool fep_immune_bridge_is_bio_async_connected(
    const fep_immune_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_IMMUNE_BRIDGE_H */
