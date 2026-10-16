/**
 * @file nimcp_core_directives_fep_bridge.h
 * @brief Free Energy Principle - Core Directives Integration Bridge
 * @version 1.0.0
 * @date 2025-12-16
 *
 * WHAT: Bidirectional integration between FEP and Core Directives system
 * WHY:  Safety directives constrain action selection under active inference.
 *       Blocked actions = high prediction error (unexpected constraint).
 *       Allowed actions = low free energy (expected safe behavior).
 * HOW:  Directives → FEP action priors; FEP → directive policy evaluation
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SAFETY AS PRIOR PREFERENCES:
 * -----------------------------
 * The brain has built-in safety constraints (e.g., avoid pain, protect body):
 * - Ventromedial Prefrontal Cortex (vmPFC) encodes safety-value priors
 * - Amygdala provides threat detection → high prediction error for dangerous actions
 * - Anterior Cingulate Cortex (ACC) detects conflicts between goals and safety
 * - Safety constraints = hard priors on action selection p(a|safe) vs p(a|unsafe)
 *
 * PREDICTION ERROR FROM BLOCKED ACTIONS:
 * --------------------------------------
 * When an action is blocked by safety directives:
 * - High prediction error (agent expected action to be viable)
 * - Surprise signal triggers policy re-evaluation
 * - Active inference searches for alternative safe actions
 * - Learning: Update generative model to avoid unsafe action proposals
 *
 * COMBINATORIAL HARM AS EMERGENT SURPRISE:
 * ----------------------------------------
 * Individually safe actions that combine into harm:
 * - High surprise (unexpected emergent behavior)
 * - Epistemic foraging to understand causal interactions
 * - Model refinement to capture higher-order dependencies
 *
 * PRECISION-WEIGHTED DIRECTIVES:
 * ------------------------------
 * Not all directives have equal weight:
 * - High precision = strong safety constraint (e.g., "don't kill")
 * - Low precision = soft guideline (e.g., "prefer efficiency")
 * - Precision modulates influence on action selection via expected free energy
 *
 * FREE ENERGY FORMULATION:
 * ------------------------
 * Expected Free Energy (EFE) for action π:
 *   G(π) = E_Q[ln Q(o_τ|π) - ln P(o_τ|C)] - E_Q[D_KL[Q(s_τ|π) || Q(s_τ)]]
 *
 * Where safety directives modify:
 *   P(o_τ|C) ← Prior preferences (safe outcomes have high P)
 *   Q(o_τ|π) ← Predicted outcomes (directives constrain feasible outcomes)
 *
 * For blocked actions:
 *   G(π_blocked) → ∞ (infinite free energy = never select)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              CORE DIRECTIVES ↔ FREE ENERGY PRINCIPLE BRIDGE               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   Core Directives                          Free Energy Principle          ║
 * ║   ────────────────                          ─────────────────────         ║
 * ║                                                                            ║
 * ║   ┌─────────────────┐                      ┌──────────────────┐           ║
 * ║   │ Safety Rules    │ ──Action Priors───→  │ Policy Selection │           ║
 * ║   │ (Hard/Soft)     │                      │ (min EFE)        │           ║
 * ║   └─────────────────┘                      └──────────────────┘           ║
 * ║            ↑                                         │                     ║
 * ║            │                                         │                     ║
 * ║   Prediction Error                      Action Proposal                   ║
 * ║   (Blocked Action)                      (Evaluate Safety)                 ║
 * ║            │                                         │                     ║
 * ║            │                                         ↓                     ║
 * ║   ┌─────────────────┐                      ┌──────────────────┐           ║
 * ║   │ Model Update    │ ←─Update Model────   │ Outcome Pred.    │           ║
 * ║   │ (Learn Safety)  │                      │ (Check Harm)     │           ║
 * ║   └─────────────────┘                      └──────────────────┘           ║
 * ║                                                                            ║
 * ║   FEP STATE:                                                               ║
 * ║   ─────────────────────────────────────────────────────────────────       ║
 * ║   • Free Energy:     Expected + KL[Q||P]                                  ║
 * ║   • Prediction Error: δ = o_actual - E[o|policy]                          ║
 * ║   • Surprise:        -ln P(o|model)   (blocked action)                    ║
 * ║   • Precision:       Confidence in directive enforcement                  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * INTEGRATION PATTERNS:
 * ---------------------
 * 1. Action Allowed (Low Free Energy):
 *    - Expected outcome matches preferences
 *    - Low prediction error → model is accurate
 *    - Precision remains stable
 *
 * 2. Action Blocked (High Prediction Error):
 *    - Unexpected constraint violation
 *    - Spike in free energy → policy re-evaluation
 *    - Increase precision on violated directive
 *
 * 3. Combinatorial Harm (High Surprise):
 *    - Individually safe actions combine badly
 *    - Unexpected emergent outcome → model surprise
 *    - Epistemic drive to understand interaction
 *
 * DESIGN PATTERNS:
 * - Bridge: Connects two heterogeneous systems (Directives ↔ FEP)
 * - Observer: Tracks action outcomes for FEP updates
 * - Strategy: Different handling for blocked vs allowed actions
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via nimcp_mutex_t
 * - nimcp_malloc/nimcp_free memory management
 *
 * REFERENCES:
 * - Friston, K. (2010) "The free-energy principle: a unified brain theory?"
 * - Friston et al. (2015) "Active inference and epistemic value"
 * - Parr, Pezzulo, Friston (2022) "Active Inference: The Free Energy Principle"
 * - Da Costa et al. (2020) "Active inference on discrete state-spaces"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORE_DIRECTIVES_FEP_BRIDGE_H
#define NIMCP_CORE_DIRECTIVES_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DIRECTIVE_FEP_PREDICTION_ERROR_WEIGHT_DEFAULT  1.0f
#define DIRECTIVE_FEP_SURPRISE_THRESHOLD_DEFAULT       0.8f
#define DIRECTIVE_FEP_PRECISION_DEFAULT                0.9f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct directive_fep_bridge directive_fep_bridge_t;

/* Opaque core directives type (avoid circular dependency) */
typedef void core_directives_t;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * @brief Directive FEP bridge configuration
 *
 * WHAT: Configuration parameters for FEP-Directives integration
 * WHY:  Tunable parameters for prediction error, surprise, and precision
 * HOW:  Simple struct with sensible defaults
 */
typedef struct {
    float prediction_error_weight;    /**< Weight for prediction errors (0-2, default 1.0) */
    float surprise_threshold;         /**< Threshold for unexpected actions (0-1, default 0.8) */
    float precision_default;          /**< Default precision weighting (0-1, default 0.9) */
    bool enable_active_inference;     /**< Use active inference for decisions (default true) */
} directive_fep_config_t;

/* ============================================================================
 * State
 * ============================================================================ */

/**
 * @brief FEP state for directive system
 *
 * WHAT: Current free energy state of directive enforcement
 * WHY:  Tracks how well directives align with FEP predictions
 * HOW:  Computed from recent action history and outcomes
 */
typedef struct {
    float free_energy;               /**< Current free energy estimate */
    float prediction_error;          /**< Error from expected behavior */
    float surprise;                  /**< Surprise at action outcome */
    float precision;                 /**< Confidence in predictions */
} directive_fep_state_t;

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * @brief Directive FEP bridge statistics
 *
 * WHAT: Performance metrics for FEP-Directives integration
 * WHY:  Monitor effectiveness of directive enforcement under FEP
 * HOW:  Cumulative counters and averages
 */
typedef struct {
    uint64_t total_actions_evaluated; /**< Total actions evaluated */
    uint64_t actions_blocked_count;   /**< Actions blocked by directives */
    uint64_t actions_allowed_count;   /**< Actions allowed by directives */
    uint64_t high_surprise_count;     /**< Combinatorial harm detections */
    float avg_free_energy;            /**< Average free energy */
    float avg_prediction_error;       /**< Average prediction error */
    float avg_surprise;               /**< Average surprise */
} directive_fep_stats_t;

/* ============================================================================
 * Main Bridge Structure
 * ============================================================================ */

/**
 * @brief Directive FEP bridge
 *
 * WHAT: Integration layer between core directives and FEP
 * WHY:  Enables FEP-based reasoning about directive enforcement
 * HOW:  Maintains connections to both systems, computes FEP metrics
 */
struct directive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    directive_fep_config_t config;    /**< Configuration */
    directive_fep_state_t state;      /**< Current FEP state */
    directive_fep_stats_t stats;      /**< Statistics */

    /* System connections */
    core_directives_t* core_directives; /**< Core directives system */
    fep_system_t* fep_orchestrator;     /**< FEP orchestrator */

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Populate config with sensible defaults
 * WHY:  Simplifies initialization for common use cases
 * HOW:  Sets prediction_error_weight=1.0, surprise_threshold=0.8, etc.
 *
 * @param config Configuration structure to populate
 * @return 0 on success, NIMCP_ERROR_NULL_POINTER on failure
 */
int directive_fep_bridge_default_config(directive_fep_config_t* config);

/**
 * @brief Create directive FEP bridge
 *
 * WHAT: Allocates and initializes FEP bridge
 * WHY:  Required to connect directives with FEP system
 * HOW:  Allocates struct, initializes mutex, sets default state
 *
 * @param config Configuration (NULL for defaults)
 * @param core_directives Core directives system (can be NULL, set later)
 * @param fep_orchestrator FEP orchestrator (can be NULL, set later)
 * @return Initialized bridge or NULL on failure
 */
directive_fep_bridge_t* directive_fep_bridge_create(
    const directive_fep_config_t* config,
    core_directives_t* core_directives,
    fep_system_t* fep_orchestrator
);

/**
 * @brief Destroy directive FEP bridge
 *
 * WHAT: Frees all resources associated with bridge
 * WHY:  Prevents memory leaks on shutdown
 * HOW:  Disconnects bio-async, destroys mutex, frees struct
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void directive_fep_bridge_destroy(directive_fep_bridge_t* bridge);

/* ============================================================================
 * FEP Computation API
 * ============================================================================ */

/**
 * @brief Update FEP state
 *
 * WHAT: Recompute free energy, prediction error, surprise
 * WHY:  Tracks how directives align with FEP expectations
 * HOW:  Analyzes recent action history, computes FEP metrics
 *
 * Biological basis: Continuous monitoring of prediction errors in ACC,
 * updating generative model to minimize free energy.
 *
 * @param bridge Bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_update(directive_fep_bridge_t* bridge);

/**
 * @brief Compute free energy for action-result pair
 *
 * WHAT: Calculate free energy given action and outcome
 * WHY:  Quantifies alignment between predicted and actual outcomes
 * HOW:  G = E[ln Q(o|π) - ln P(o|C)] where C = safety preferences
 *
 * Biological basis: Expected free energy (EFE) computation in vmPFC,
 * evaluating action policies against value-based priors.
 *
 * @param bridge Bridge
 * @param action Action identifier (from directives)
 * @param result Action result (blocked=1, allowed=0)
 * @param free_energy_out Output: computed free energy
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_compute_free_energy(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    uint32_t result,
    float* free_energy_out
);

/**
 * @brief Compute expected outcome for action
 *
 * WHAT: Predict whether action will be blocked or allowed
 * WHY:  Active inference requires predicting action outcomes
 * HOW:  Uses FEP generative model to predict directive response
 *
 * @param bridge Bridge
 * @param action Action identifier
 * @param expected_blocked Output: predicted outcome (true=blocked)
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_compute_expected_outcome(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    bool* expected_blocked
);

/**
 * @brief Update precision for directive predictions
 *
 * WHAT: Adjust confidence in directive enforcement predictions
 * WHY:  Precision-weighting modulates influence of prediction errors
 * HOW:  Higher precision = stronger influence on action selection
 *
 * Biological basis: Precision encoding in neuromodulatory systems
 * (dopamine, norepinephrine) adjusts gain on prediction errors.
 *
 * @param bridge Bridge
 * @param precision New precision value (0-1)
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_update_precision(
    directive_fep_bridge_t* bridge,
    float precision
);

/* ============================================================================
 * Event Handlers
 * ============================================================================ */

/**
 * @brief Handle blocked action event
 *
 * WHAT: Update FEP state when directive blocks an action
 * WHY:  Blocked action = high prediction error (unexpected constraint)
 * HOW:  Increment error, update surprise, adjust precision
 *
 * Biological basis: Amygdala detects safety violation, sends surprise
 * signal to ACC which updates action policy via prediction error.
 *
 * @param bridge Bridge
 * @param action Action that was blocked
 * @param reason Reason code for blocking
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_on_action_blocked(
    directive_fep_bridge_t* bridge,
    uint32_t action,
    uint32_t reason
);

/**
 * @brief Handle allowed action event
 *
 * WHAT: Update FEP state when directive allows an action
 * WHY:  Allowed action = low free energy (expected safe behavior)
 * HOW:  Reduce prediction error, maintain precision
 *
 * @param bridge Bridge
 * @param action Action that was allowed
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_on_action_allowed(
    directive_fep_bridge_t* bridge,
    uint32_t action
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current FEP state
 *
 * WHAT: Retrieve current free energy metrics
 * WHY:  Monitoring and debugging of FEP integration
 * HOW:  Copies state struct under mutex protection
 *
 * @param bridge Bridge
 * @param state Output state structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_get_state(
    const directive_fep_bridge_t* bridge,
    directive_fep_state_t* state
);

/**
 * @brief Get statistics
 *
 * WHAT: Retrieve cumulative statistics
 * WHY:  Monitor effectiveness of FEP-Directives integration
 * HOW:  Copies stats struct under mutex protection
 *
 * @param bridge Bridge
 * @param stats Output statistics structure
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_get_stats(
    const directive_fep_bridge_t* bridge,
    directive_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async for cross-module messaging
 * WHY:  Enables coordination with other FEP bridges
 * HOW:  Registers with BIO_MODULE_FEP_CORE_DIRECTIVES, sets up inbox
 *
 * @param bridge Bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_connect_bio_async(directive_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Cleanup before shutdown
 * HOW:  Calls bio_router_unregister_module
 *
 * @param bridge Bridge
 * @return 0 on success, NIMCP_ERROR_* on failure
 */
int directive_fep_bridge_disconnect_bio_async(directive_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * WHAT: Returns whether bridge is connected to bio-async
 * WHY:  Enables conditional bio-async operations
 * HOW:  Returns bio_async_enabled flag
 *
 * @param bridge Bridge
 * @return true if connected, false otherwise
 */
bool directive_fep_bridge_is_bio_async_connected(
    const directive_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORE_DIRECTIVES_FEP_BRIDGE_H */
