/**
 * @file nimcp_brain_immune_fep_bridge.h
 * @brief FEP Bridge for Brain Immune System
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between brain immune system and Free Energy Principle
 * WHY:  Model immune system as allostatic regulator minimizing free energy via
 *       cytokine-driven precision weighting and inflammation as prediction failure
 * HOW:  Cytokine signals map to precision weights, inflammation maps to prediction error,
 *       FEP beliefs guide threat assessment, immune responses minimize expected free energy
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE SYSTEM AS FREE ENERGY MINIMIZER:
 * ---------------------------------------
 * The immune system can be understood through the Free Energy Principle framework:
 *
 * 1. Generative Model:
 *    - Prior beliefs: Self vs non-self distinction (tolerance system)
 *    - Likelihood: Threat signatures (B cell receptors, T cell receptors)
 *    - Hidden states: True pathogen status (pathogenic vs benign)
 *
 * 2. Observations:
 *    - Antigen presentation (threat signatures detected)
 *    - Pattern recognition (BBB, BFT, swarm immune detection)
 *    - Cytokine concentrations (signaling molecules)
 *
 * 3. Prediction Errors:
 *    - Inflammation = Failed prediction (immune activation)
 *    - Tolerance = Accurate prediction (no response needed)
 *    - Autoimmunity = False positive (predicting threat where none exists)
 *
 * 4. Precision Weighting:
 *    - Pro-inflammatory cytokines (IL-1, IL-6, TNF-α) ↑ precision → stronger immune response
 *    - Anti-inflammatory cytokines (IL-10) ↓ precision → immune suppression
 *    - Precision modulates trust in threat signals
 *
 * 5. Active Inference:
 *    - Immune responses (antibodies, killer T cells) = Actions to minimize expected free energy
 *    - Memory formation = Model updating to improve future predictions
 *    - Cytokine release = Precision optimization
 *
 * CYTOKINE-PRECISION MAPPING:
 * ---------------------------
 * Pro-inflammatory (↑ Precision):
 * - IL-1β (CYTOKINE_IL1_BRAIN) → Increase precision of threat signals
 * - IL-6 (CYTOKINE_IL6) → Amplify prediction error magnitude
 * - TNF-α (CYTOKINE_TNF) → Strong precision boost for severe threats
 * - IFN-γ (CYTOKINE_IFN_GAMMA) → Precision for viral-like threats
 *
 * Anti-inflammatory (↓ Precision):
 * - IL-10 (CYTOKINE_IL10) → Reduce precision, suppress immune response
 *
 * INFLAMMATION AS PREDICTION ERROR:
 * ---------------------------------
 * - INFLAMMATION_NONE: Prediction accurate, no error
 * - INFLAMMATION_LOCAL: Small prediction error, local correction
 * - INFLAMMATION_REGIONAL: Moderate error, regional adjustments
 * - INFLAMMATION_SYSTEMIC: Large prediction error, system-wide update
 * - INFLAMMATION_STORM: Catastrophic prediction failure (cytokine storm)
 *
 * FEP-IMMUNE INTEGRATION:
 * ======================
 * ```
 * FEP System                    Brain Immune System
 * ──────────────────────────────────────────────────────────────
 * Beliefs about hidden states → Self/non-self discrimination
 * Prediction errors           → Inflammation levels
 * Precision weights           → Cytokine concentrations
 * Variational free energy     → Immune system entropy
 * Expected free energy        → Cost of immune responses
 * Active inference (actions)  → Antibody production, T cell killing
 * Model updating              → Memory B/T cell formation
 * Sensory attenuation         → Tolerance (ignore benign stimuli)
 * ```
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects FEP and brain immune systems
 * - Observer: Monitors cytokine signals and inflammation
 * - Strategy: Pluggable precision-weighting strategies
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_IMMUNE_FEP_BRIDGE_H
#define NIMCP_BRAIN_IMMUNE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine-precision mapping weights */
#define BRAIN_IMMUNE_FEP_IL1_PRECISION_WEIGHT      1.5f   /**< IL-1β precision boost */
#define BRAIN_IMMUNE_FEP_IL6_PRECISION_WEIGHT      1.8f   /**< IL-6 precision boost */
#define BRAIN_IMMUNE_FEP_TNF_PRECISION_WEIGHT      2.0f   /**< TNF-α precision boost */
#define BRAIN_IMMUNE_FEP_IFN_PRECISION_WEIGHT      1.6f   /**< IFN-γ precision boost */
#define BRAIN_IMMUNE_FEP_IL10_PRECISION_WEIGHT     0.5f   /**< IL-10 precision reduction */

/* Inflammation-to-prediction-error scaling */
#define BRAIN_IMMUNE_FEP_INFLAMMATION_SCALE        2.0f   /**< Inflammation → prediction error */
#define BRAIN_IMMUNE_FEP_MAX_PREDICTION_ERROR      10.0f  /**< Max prediction error */

/* Free energy thresholds */
#define BRAIN_IMMUNE_FEP_THREAT_FE_THRESHOLD       5.0f   /**< Free energy for threat detection */
#define BRAIN_IMMUNE_FEP_TOLERANCE_FE_THRESHOLD    2.0f   /**< Free energy for tolerance */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct brain_immune_fep_bridge brain_immune_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief FEP effects on brain immune system
 *
 * How FEP beliefs and predictions influence immune responses.
 */
typedef struct {
    float threat_assessment;          /**< FEP-based threat probability (0-1) */
    float tolerance_confidence;       /**< Confidence in tolerance (0-1) */
    float response_magnitude;         /**< Suggested response strength (0-1) */
    float memory_formation_signal;    /**< Signal to form memory (0-1) */
} brain_immune_fep_effects_t;

/**
 * @brief Immune effects on FEP system
 *
 * How immune signals (cytokines, inflammation) influence FEP.
 */
typedef struct {
    float precision_modulation;       /**< Overall precision adjustment (0-3) */
    float prediction_error_magnitude; /**< Inflammation-driven error (0-10) */
    float il1_precision;              /**< IL-1β contribution to precision */
    float il6_precision;              /**< IL-6 contribution to precision */
    float tnf_precision;              /**< TNF-α contribution to precision */
    float ifn_precision;              /**< IFN-γ contribution to precision */
    float il10_suppression;           /**< IL-10 precision suppression */
} fep_immune_effects_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Precision modulation */
    float cytokine_precision_scale;   /**< Global cytokine-precision scaling */
    bool enable_precision_modulation; /**< Enable cytokine precision effects */

    /* Prediction error mapping */
    float inflammation_error_scale;   /**< Inflammation-to-error scaling */
    bool enable_inflammation_errors;  /**< Enable inflammation prediction errors */

    /* FEP guidance */
    float fep_threat_threshold;       /**< Free energy threshold for threats */
    float fep_tolerance_threshold;    /**< Free energy threshold for tolerance */
    bool enable_fep_guided_responses; /**< Enable FEP-guided immune responses */

    /* Integration */
    bool enable_bio_async;            /**< Enable bio-async messaging */
    bool enable_logging;              /**< Enable logging */
} brain_immune_fep_config_t;

/**
 * @brief Bridge state tracking
 */
typedef struct {
    uint64_t total_updates;           /**< Total bridge updates */
    uint64_t cytokine_modulations;    /**< Cytokine precision updates */
    uint64_t inflammation_signals;    /**< Inflammation prediction errors */
    uint64_t fep_guided_responses;    /**< FEP-guided immune actions */
    float avg_precision_modulation;   /**< Average precision modulation */
    float avg_prediction_error;       /**< Average prediction error */
} brain_immune_fep_state_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint64_t precision_modulations;
    uint64_t prediction_errors_generated;
    uint64_t fep_guided_actions;
    float current_precision_modulation;
    float current_prediction_error;
} brain_immune_fep_stats_t;

/**
 * @brief Brain immune FEP bridge
 */
struct brain_immune_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    brain_immune_fep_config_t config; /**< Configuration */
    fep_system_t* fep_system;         /**< FEP system */
    brain_immune_system_t* immune_system; /**< Brain immune system */

    /* Computed effects */
    brain_immune_fep_effects_t fep_effects; /**< FEP → immune effects */
    fep_immune_effects_t immune_effects;    /**< Immune → FEP effects */

    /* State */
    brain_immune_fep_state_t state;   /**< Bridge state */
    brain_immune_fep_stats_t stats;   /**< Statistics */

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default bridge configuration
 * WHY:  Easy initialization with biologically-motivated parameters
 * HOW:  Return struct with balanced cytokine-precision mappings
 *
 * @param config Output configuration
 * @return 0 on success
 */
int brain_immune_fep_default_config(brain_immune_fep_config_t* config);

/**
 * @brief Create FEP bridge for brain immune system
 *
 * WHAT: Initialize bidirectional FEP-immune integration
 * WHY:  Enable allostatic immune regulation via free energy minimization
 * HOW:  Connect FEP and immune systems, set up precision-cytokine mappings
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param fep_system FEP system
 * @return New bridge or NULL on failure
 */
brain_immune_fep_bridge_t* brain_immune_fep_create(
    const brain_immune_fep_config_t* config,
    brain_immune_system_t* immune_system,
    fep_system_t* fep_system
);

/**
 * @brief Destroy FEP bridge
 *
 * @param bridge Bridge to destroy
 */
void brain_immune_fep_destroy(brain_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update bridge state
 *
 * WHAT: Compute bidirectional FEP-immune effects
 * WHY:  Synchronize FEP precision and immune responses
 * HOW:  Map cytokines to precision, inflammation to errors, FEP beliefs to responses
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int brain_immune_fep_update(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Apply FEP effects to immune system
 *
 * WHAT: Use FEP beliefs to guide immune responses
 * WHY:  Threat assessment via free energy minimization
 * HOW:  Adjust response magnitude based on FEP predictions
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int brain_immune_fep_apply_to_immune(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Apply immune effects to FEP system
 *
 * WHAT: Modulate FEP precision via cytokine signals
 * WHY:  Cytokines encode confidence/uncertainty in threat signals
 * HOW:  Update FEP precision weights based on cytokine concentrations
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int brain_immune_fep_apply_to_fep(brain_immune_fep_bridge_t* bridge);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current precision modulation
 *
 * @param bridge FEP bridge
 * @return Precision modulation factor (0-3)
 */
float brain_immune_fep_get_precision_modulation(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Get current prediction error from inflammation
 *
 * @param bridge FEP bridge
 * @return Prediction error magnitude (0-10)
 */
float brain_immune_fep_get_prediction_error(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Get FEP threat assessment
 *
 * @param bridge FEP bridge
 * @param antigen_id Antigen to assess
 * @param threat_prob Output: threat probability (0-1)
 * @return 0 on success
 */
int brain_immune_fep_assess_threat(
    brain_immune_fep_bridge_t* bridge,
    uint32_t antigen_id,
    float* threat_prob
);

/**
 * @brief Get statistics
 *
 * @param bridge FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int brain_immune_fep_get_stats(
    const brain_immune_fep_bridge_t* bridge,
    brain_immune_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int brain_immune_fep_connect_bio_async(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge FEP bridge
 * @return 0 on success
 */
int brain_immune_fep_disconnect_bio_async(brain_immune_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge FEP bridge
 * @return true if connected
 */
bool brain_immune_fep_is_bio_async_connected(const brain_immune_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_FEP_BRIDGE_H */
