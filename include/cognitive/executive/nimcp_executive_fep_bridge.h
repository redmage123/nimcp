/**
 * @file nimcp_executive_fep_bridge.h
 * @brief Free Energy Principle - Executive Function Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and executive function
 * WHY:  Executive control implements EFE-based policy selection. Decision-making
 *       minimizes expected free energy under constraints.
 * HOW:  FEP computes EFE for policies; executive selects actions based on EFE;
 *       executive goals constrain FEP's generative model.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * EXECUTIVE FUNCTION AS ACTIVE INFERENCE:
 * ---------------------------------------
 * - Friston et al. (2017): Executive control = policy selection minimizing EFE
 * - Dorsolateral PFC maintains working memory (beliefs about states)
 * - Anterior cingulate cortex (ACC) computes prediction errors
 * - Orbitofrontal cortex (OFC) represents expected outcomes (preferences)
 *
 * FEP → EXECUTIVE PATHWAYS:
 * -------------------------
 * 1. Expected Free Energy Guides Policy Selection:
 *    - G(π) = Risk + Ambiguity
 *    - Policies with lower EFE preferred
 *    - Executive selects min-EFE actions
 *    - Reference: Friston et al. (2015) "Active inference and agency"
 *
 * 2. Precision Modulates Exploration-Exploitation:
 *    - High precision → Exploitation (confident predictions)
 *    - Low precision → Exploration (uncertain, seek info)
 *    - Temperature parameter from precision
 *    - Biological: Noradrenaline modulates cortical precision
 *
 * 3. Prediction Errors Drive Cognitive Control:
 *    - High PE → Increase cognitive control
 *    - ACC monitors PE, triggers control
 *    - Reference: Botvinick et al. (2001) "Conflict monitoring and ACC"
 *
 * EXECUTIVE → FEP PATHWAYS:
 * -------------------------
 * 1. Goals Constrain Generative Model:
 *    - Executive goals → Preferred observations p*(o)
 *    - Goal states bias FEP priors
 *    - EFE includes goal-seeking term
 *
 * 2. Working Memory Maintains Beliefs:
 *    - Executive WM holds beliefs about states
 *    - Prevents catastrophic forgetting
 *    - Biological: DLPFC sustained activity
 *
 * 3. Inhibition Implements Precision Control:
 *    - Executive inhibition → Reduce precision
 *    - Suppress irrelevant prediction errors
 *    - Biological: GABAergic inhibition
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  FEP-EXECUTIVE BRIDGE                                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                FEP → EXECUTIVE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ EXP FREE NRG │ ──→ Policy Selection                            │  ║
 * ║   │   │ G(π1) = 2.5  │     - Select policy with min G(π)               │  ║
 * ║   │   │ G(π2) = 1.8  │     - π2 preferred                              │  ║
 * ║   │   │ G(π3) = 3.2  │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PRECISION    │ ──→ Exploration-Exploitation                    │  ║
 * ║   │   │  (High Π)    │     - High → Exploitation                       │  ║
 * ║   │   │  (Low Π)     │     - Low → Exploration                         │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PRED ERRORS  │ ──→ Cognitive Control Trigger                   │  ║
 * ║   │   │  PE > 5.0    │     - High PE → Increase control                │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              EXECUTIVE → FEP PATHWAYS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ EXECUTIVE GOALS  │ ──→ Preferred Observations p*(o)            │  ║
 * ║   │   │  Goal: X=1       │     - Constrains generative model           │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ WORKING MEMORY   │ ──→ Belief Maintenance                      │  ║
 * ║   │   │  (state buffer)  │     - Prevents belief drift                 │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ INHIBITION       │ ──→ Precision Suppression                   │  ║
 * ║   │   │  (control signal)│     - Reduces precision on irrelevant PEs   │  ║
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

#ifndef NIMCP_EXECUTIVE_FEP_BRIDGE_H
#define NIMCP_EXECUTIVE_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "cognitive/nimcp_executive.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* EFE policy selection */
#define EXECUTIVE_FEP_MAX_POLICIES           32     /**< Maximum policies to evaluate */
#define EXECUTIVE_FEP_DEFAULT_TEMPERATURE    1.0f   /**< Softmax temperature */
#define EXECUTIVE_FEP_EFE_THRESHOLD          10.0f  /**< High EFE threshold */

/* Precision-exploration mapping */
#define EXECUTIVE_FEP_HIGH_PRECISION_EXPLOIT 0.9f   /**< Exploitation probability (high Π) */
#define EXECUTIVE_FEP_LOW_PRECISION_EXPLORE  0.7f   /**< Exploration probability (low Π) */
#define EXECUTIVE_FEP_PRECISION_THRESHOLD    1.0f   /**< Precision threshold */

/* Cognitive control */
#define EXECUTIVE_FEP_PE_CONTROL_THRESHOLD   5.0f   /**< PE threshold for control */
#define EXECUTIVE_FEP_CONTROL_BOOST          1.5f   /**< Control signal boost */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct executive_fep_bridge executive_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Executive-FEP bridge
 */
typedef struct {
    /* FEP → Executive */
    float efe_temperature;               /**< Softmax temperature for policy selection */
    float precision_exploration_threshold; /**< Precision threshold for exploration */
    float pe_control_threshold;          /**< PE threshold for cognitive control */
    bool enable_efe_policy_selection;    /**< Enable EFE-based policy selection */
    bool enable_precision_exploration;   /**< Enable precision-exploration mapping */
    bool enable_pe_cognitive_control;    /**< Enable PE-driven control */

    /* Executive → FEP */
    float goal_prior_strength;           /**< Strength of goal priors */
    float wm_belief_persistence;         /**< WM belief persistence factor */
    float inhibition_precision_reduction; /**< Precision reduction from inhibition */
    bool enable_goal_priors;             /**< Enable goal → prior constraints */
    bool enable_wm_belief_maintenance;   /**< Enable WM → belief maintenance */
    bool enable_inhibition_precision;    /**< Enable inhibition → precision */

    /* Sensitivity factors */
    float efe_sensitivity;               /**< EFE effect scaling */
    float executive_sensitivity;         /**< Executive effect scaling */
} executive_fep_config_t;

/**
 * @brief FEP effects on executive function
 */
typedef struct {
    /* Policy selection */
    float policy_efe[EXECUTIVE_FEP_MAX_POLICIES]; /**< EFE for each policy */
    uint32_t selected_policy;            /**< Selected policy index */
    float selection_confidence;          /**< Confidence in selection */

    /* Exploration-exploitation */
    float current_precision;             /**< Current FEP precision */
    float exploration_probability;       /**< Exploration probability */
    bool exploration_mode;               /**< Exploration vs exploitation */

    /* Cognitive control */
    float current_prediction_error;      /**< Current PE magnitude */
    float control_signal;                /**< Cognitive control signal */
    bool control_active;                 /**< Control engaged */
} executive_fep_effects_t;

/**
 * @brief Executive effects on FEP
 */
typedef struct {
    /* Goal priors */
    float goal_prior_bias;               /**< Goal prior bias value */
    bool goal_prior_active;              /**< Goal prior applied */

    /* Working memory */
    float wm_belief_strength;            /**< WM belief strength */
    bool wm_maintenance_active;          /**< WM maintenance active */

    /* Inhibition */
    float inhibition_level;              /**< Inhibition strength [0-1] */
    float precision_suppression;         /**< Precision suppression factor */
} fep_executive_effects_t;

/**
 * @brief Current state of Executive-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_efe;                   /**< Current EFE */
    float current_precision;             /**< Current precision */
    float current_prediction_error;      /**< Current PE */

    /* Applied modifiers */
    float exploration_probability;       /**< Applied exploration prob */
    float control_signal;                /**< Applied control signal */
    float goal_prior_strength;           /**< Applied goal prior */

    /* State flags */
    bool exploration_mode;               /**< Exploration active */
    bool control_active;                 /**< Cognitive control active */
    bool goal_priors_active;             /**< Goal priors applied */

    /* Timestamps */
    uint64_t last_policy_selection_time; /**< Last policy selection */
    uint64_t last_control_trigger_time;  /**< Last control trigger */
} executive_fep_state_t;

/**
 * @brief Statistics for Executive-FEP bridge
 */
typedef struct {
    /* FEP → Executive */
    uint64_t policy_selections;          /**< Total policy selections */
    uint64_t exploration_events;         /**< Exploration mode events */
    uint64_t cognitive_control_triggers; /**< Control trigger events */
    float avg_efe;                       /**< Average EFE */
    float avg_exploration_prob;          /**< Average exploration probability */

    /* Executive → FEP */
    uint64_t goal_prior_applications;    /**< Goal prior applications */
    uint64_t wm_maintenance_events;      /**< WM maintenance events */
    uint64_t inhibition_events;          /**< Inhibition events */
    float avg_goal_prior_strength;       /**< Average goal prior strength */
    float avg_inhibition_level;          /**< Average inhibition level */

    /* Performance */
    float avg_prediction_error;          /**< Average PE */
    float avg_free_energy;               /**< Average free energy */
} executive_fep_stats_t;

/**
 * @brief Executive-FEP bridge state
 */
struct executive_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    executive_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    executive_controller_t* executive_system; /**< Executive system */

    /* Current effects */
    executive_fep_effects_t fep_effects; /**< FEP → Executive */
    fep_executive_effects_t executive_effects; /**< Executive → FEP */
    executive_fep_state_t state;

    /* Statistics */
    executive_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int executive_fep_bridge_default_config(executive_fep_config_t* config);
executive_fep_bridge_t* executive_fep_bridge_create(const executive_fep_config_t* config);
void executive_fep_bridge_destroy(executive_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

int executive_fep_bridge_connect_fep(executive_fep_bridge_t* bridge, fep_system_t* fep);
int executive_fep_bridge_connect_executive(executive_fep_bridge_t* bridge, executive_controller_t* executive);
int executive_fep_bridge_disconnect(executive_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Executive Direction
 * ============================================================================ */

int executive_fep_select_policy_by_efe(executive_fep_bridge_t* bridge);
int executive_fep_modulate_exploration(executive_fep_bridge_t* bridge);
int executive_fep_trigger_cognitive_control(executive_fep_bridge_t* bridge, float pe_magnitude);

/* ============================================================================
 * Executive → FEP Direction
 * ============================================================================ */

int executive_fep_apply_goal_priors(executive_fep_bridge_t* bridge);
int executive_fep_maintain_wm_beliefs(executive_fep_bridge_t* bridge);
int executive_fep_apply_inhibition_precision(executive_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

int executive_fep_bridge_update(executive_fep_bridge_t* bridge, uint64_t delta_ms);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

int executive_fep_bridge_get_state(executive_fep_bridge_t* bridge, executive_fep_state_t* state);
int executive_fep_bridge_get_stats(executive_fep_bridge_t* bridge, executive_fep_stats_t* stats);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

int executive_fep_bridge_connect_bio_async(executive_fep_bridge_t* bridge);
int executive_fep_bridge_disconnect_bio_async(executive_fep_bridge_t* bridge);
bool executive_fep_bridge_is_bio_async_connected(const executive_fep_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_EXECUTIVE_FEP_BRIDGE_H */
