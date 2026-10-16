/**
 * @file nimcp_plasticity_substrate_bridge.h
 * @brief Plasticity-Neural Substrate Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between plasticity mechanisms and neural substrate
 * WHY:  Learning requires energy (ATP), depends on temperature, and is affected by
 *       cellular health. Substrate provides the physical platform for plasticity.
 * HOW:  Substrate modulates learning rates, STDP windows, BCM thresholds, homeostatic
 *       set points, and eligibility trace decay based on metabolic/physical state.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SUBSTRATE → PLASTICITY PATHWAYS:
 * --------------------------------
 * 1. Energy-Dependent LTP/LTD:
 *    - AMPA receptor trafficking requires ATP (100-1000 ATP per receptor)
 *    - Protein synthesis for L-LTP requires metabolic energy
 *    - ATP depletion blocks LTP induction (Bollen et al. 2016)
 *    - Reference: Harris & Teyler (1984) "Age differences in LTP"
 *
 * 2. Temperature Effects on Synaptic Kinetics:
 *    - NMDA receptor kinetics have Q10 ≈ 2.2
 *    - Temperature affects STDP timing window (Bi & Poo 1998)
 *    - Hyperthermia shifts toward LTD (Chen et al. 2000)
 *    - Reference: Andersen & Moser (1995) "Temperature effects"
 *
 * 3. Metabolic Stress and BCM Threshold:
 *    - Low energy shifts BCM threshold toward LTD (protection)
 *    - ATP depletion reduces postsynaptic calcium signaling
 *    - Chronic stress impairs synaptic scaling (Chen et al. 2013)
 *    - Reference: Kim & Diamond (2002) "Stress impairs plasticity"
 *
 * 4. Homeostatic Plasticity and Substrate Health:
 *    - Synaptic scaling adjusts to metabolic constraints
 *    - Intrinsic plasticity compensates for ionic imbalances
 *    - Substrate degradation triggers compensatory mechanisms
 *    - Reference: Turrigiano (2008) "Homeostatic synaptic plasticity"
 *
 * 5. Eligibility Traces and ATP:
 *    - Trace persistence requires active maintenance (ATP cost)
 *    - Energy depletion accelerates trace decay
 *    - Consolidation (trace→weight) requires protein synthesis (ATP-dependent)
 *    - Reference: Frey & Morris (1997) "Synaptic tagging and capture"
 *
 * 6. Dendritic Plasticity and Membrane Integrity:
 *    - NMDA receptor function depends on membrane potential
 *    - Mg²⁺ block relief requires healthy membrane
 *    - Dendritic spikes require intact voltage-gated channels
 *    - Reference: Spruston (2008) "Dendritic integration"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║           PLASTICITY-SUBSTRATE INTEGRATION BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │            SUBSTRATE → PLASTICITY MODULATION                        │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐   ┌──────────────┐   ┌──────────────┐          │  ║
 * ║   │   │ ATP Level    │   │ Temperature  │   │ Membrane     │          │  ║
 * ║   │   │ ──────────── │   │ ──────────── │   │ ──────────── │          │  ║
 * ║   │   │ → LR scaling │   │ → STDP       │   │ → Dendritic  │          │  ║
 * ║   │   │ → Trace      │   │   window     │   │   function   │          │  ║
 * ║   │   │   decay      │   │ → BCM shift  │   │              │          │  ║
 * ║   │   └──────┬───────┘   └──────┬───────┘   └──────┬───────┘          │  ║
 * ║   │          └──────────────────┴──────────────────┘                   │  ║
 * ║   │                             ↓                                      │  ║
 * ║   │   ┌─────────────────────────────────────────────────────────────┐ │  ║
 * ║   │   │              PLASTICITY MODULATION EFFECTS                  │ │  ║
 * ║   │   │  • Learning rate: LR × plasticity_capacity                  │ │  ║
 * ║   │   │  • STDP window: τ × Q10^((T-37)/10)                         │ │  ║
 * ║   │   │  • BCM threshold: θ × metabolic_factor                      │ │  ║
 * ║   │   │  • Homeostatic set point: adjusted by substrate health      │ │  ║
 * ║   │   │  • Eligibility decay: λ × (1 - ATP_deficit)                 │ │  ║
 * ║   │   │  • Dendritic NMDA: gated by membrane integrity              │ │  ║
 * ║   │   └─────────────────────────────────────────────────────────────┘ │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                 PLASTICITY MECHANISMS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────┐  ┌──────┐  ┌─────────┐  ┌──────────┐  ┌──────────┐    │  ║
 * ║   │   │ STDP │  │ BCM  │  │Homeosta │  │Eligibili │  │Dendritic │    │  ║
 * ║   │   │      │  │      │  │  tic    │  │   ty     │  │          │    │  ║
 * ║   │   └──────┘  └──────┘  └─────────┘  └──────────┘  └──────────┘    │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * DESIGN PATTERNS:
 * - Facade Pattern: Unified interface to all plasticity mechanisms
 * - Strategy Pattern: Different modulation strategies per plasticity type
 * - Observer Pattern: Substrate monitors plasticity activity
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PLASTICITY_SUBSTRATE_BRIDGE_H
#define NIMCP_PLASTICITY_SUBSTRATE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/neural_substrate/nimcp_neural_substrate.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/dendritic/nimcp_dendritic.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

/* Thread utilities */
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Q10 temperature coefficients for plasticity (biological measurements) */
#define PLASTICITY_Q10_STDP                2.2f    /**< STDP: Q10 ≈ 2.2 (Bi & Poo 1998) */
#define PLASTICITY_Q10_BCM                 2.0f    /**< BCM: Q10 ≈ 2.0 */
#define PLASTICITY_Q10_NMDA                2.3f    /**< NMDA: Q10 ≈ 2.3 (Jahr & Stevens) */
#define PLASTICITY_Q10_ELIGIBILITY         1.8f    /**< Eligibility: Q10 ≈ 1.8 */

/* Reference temperature (normal brain) */
#define PLASTICITY_REFERENCE_TEMP          37.0f   /**< Normal brain temp (°C) */

/* ATP thresholds for plasticity */
#define PLASTICITY_ATP_FULL                0.8f    /**< ATP > 0.8: Full plasticity */
#define PLASTICITY_ATP_REDUCED             0.5f    /**< ATP < 0.5: Reduced plasticity */
#define PLASTICITY_ATP_BLOCKED             0.3f    /**< ATP < 0.3: LTP blocked */

/* Metabolic modulation ranges */
#define PLASTICITY_LR_MIN_FACTOR           0.1f    /**< Minimum LR multiplier */
#define PLASTICITY_LR_MAX_FACTOR           1.5f    /**< Maximum LR multiplier (hypermetabolic) */
#define PLASTICITY_STDP_WINDOW_MIN         0.5f    /**< Minimum STDP window factor */
#define PLASTICITY_STDP_WINDOW_MAX         2.0f    /**< Maximum STDP window factor */

/* BCM threshold modulation */
#define PLASTICITY_BCM_STRESS_SHIFT        1.3f    /**< Metabolic stress → 30% higher threshold (bias LTD) */
#define PLASTICITY_BCM_OPTIMAL_SHIFT       1.0f    /**< Optimal substrate → normal threshold */
#define PLASTICITY_BCM_HYPOXIA_SHIFT       1.5f    /**< Hypoxia → 50% higher threshold */

/* Homeostatic adjustment factors */
#define PLASTICITY_HOMEOSTATIC_DEGRADED    0.7f    /**< Degraded substrate → lower target rate */
#define PLASTICITY_HOMEOSTATIC_OPTIMAL     1.0f    /**< Optimal substrate → normal target rate */

/* Eligibility trace modulation */
#define PLASTICITY_ELIGIBILITY_ATP_FACTOR  0.3f    /**< ATP contribution to trace decay */
#define PLASTICITY_ELIGIBILITY_TEMP_FACTOR 0.2f    /**< Temperature contribution to trace decay */

/* Dendritic modulation */
#define PLASTICITY_DENDRITIC_MEMBRANE_MIN  0.6f    /**< Min membrane integrity for NMDA */
#define PLASTICITY_DENDRITIC_ION_MIN       0.5f    /**< Min ion balance for dendritic spikes */

/* Bio-async module ID */
#define BIO_MODULE_PLASTICITY_SUBSTRATE    0x0D30  /**< Bio-async module ID */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Substrate effects on STDP
 */
typedef struct {
    float learning_rate_mod;     /**< LR multiplier [0.1-1.5] */
    float tau_plus_mod;          /**< LTP window multiplier */
    float tau_minus_mod;         /**< LTD window multiplier */
    float temperature_factor;    /**< Q10-based temperature effect */
    float atp_gating;            /**< ATP-based gating [0-1] */
} stdp_substrate_effects_t;

/**
 * @brief Substrate effects on BCM
 */
typedef struct {
    float threshold_shift;       /**< BCM θ multiplier */
    float learning_rate_mod;     /**< LR multiplier */
    float metabolic_bias;        /**< Bias toward LTP/LTD [-1,1] */
    float stability_factor;      /**< Stability of threshold sliding */
} bcm_substrate_effects_t;

/**
 * @brief Substrate effects on homeostatic plasticity
 */
typedef struct {
    float target_rate_adjustment; /**< Target firing rate adjustment */
    float scaling_rate_mod;       /**< Scaling rate multiplier */
    float ip_threshold_shift;     /**< Intrinsic plasticity threshold shift */
    float recovery_boost;         /**< Recovery rate boost [0-1] */
} homeostatic_substrate_effects_t;

/**
 * @brief Substrate effects on eligibility traces
 */
typedef struct {
    float decay_lambda_mod;      /**< Trace decay multiplier */
    float consolidation_gate;    /**< Consolidation gating [0-1] */
    float atp_maintenance;       /**< ATP-dependent maintenance [0-1] */
    float protein_synthesis_rate; /**< Protein synthesis rate [0-1] */
} eligibility_substrate_effects_t;

/**
 * @brief Substrate effects on dendritic plasticity
 */
typedef struct {
    float nmda_conductance_mod;  /**< NMDA conductance multiplier */
    float spike_threshold_shift; /**< Dendritic spike threshold shift */
    float calcium_influx_mod;    /**< Ca²⁺ influx multiplier */
    float membrane_factor;       /**< Membrane integrity gating [0-1] */
} dendritic_substrate_effects_t;

/**
 * @brief Combined substrate effects on all plasticity mechanisms
 */
typedef struct {
    stdp_substrate_effects_t stdp;
    bcm_substrate_effects_t bcm;
    homeostatic_substrate_effects_t homeostatic;
    eligibility_substrate_effects_t eligibility;
    dendritic_substrate_effects_t dendritic;

    /* Global modulation */
    float global_learning_rate;  /**< Global LR multiplier [0-1] */
    float plasticity_capacity;   /**< Overall plasticity capacity [0-1] */
} plasticity_substrate_effects_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;
    uint32_t atp_limited_events;
    uint32_t temperature_modulations;
    uint32_t membrane_blocks;
    float min_learning_rate_factor;
    float max_learning_rate_factor;
    float avg_plasticity_capacity;
} plasticity_substrate_stats_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_stdp_modulation;
    bool enable_bcm_modulation;
    bool enable_homeostatic_modulation;
    bool enable_eligibility_modulation;
    bool enable_dendritic_modulation;
    bool enable_bio_async;

    /* Sensitivity multipliers */
    float atp_sensitivity;          /**< How strongly ATP affects LR [0-2] */
    float temperature_sensitivity;  /**< How strongly temp affects windows [0-2] */
    float membrane_sensitivity;     /**< How strongly membrane affects dendrites [0-2] */

    /* Biological realism flags */
    bool enforce_atp_blocking;      /**< Block LTP at low ATP (strict) */
    bool use_q10_temperature;       /**< Use Q10 temperature scaling */
    bool compensate_homeostatic;    /**< Homeostatic compensation for substrate */
} plasticity_substrate_config_t;

/**
 * @brief Complete plasticity-substrate bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handle */
    neural_substrate_t* substrate;

    /* Plasticity mechanism handles (optional, for direct access) */
    void* stdp_context;              /**< User-provided STDP context */
    void* bcm_context;               /**< User-provided BCM context */
    homeostatic_controller_t homeostatic_controller;
    void* eligibility_context;       /**< User-provided eligibility context */
    dendritic_tree_t dendritic_tree;

    /* Current effects */
    plasticity_substrate_effects_t effects;

    /* Configuration */
    plasticity_substrate_config_t config;

    /* Statistics */
    plasticity_substrate_stats_t stats;

    } plasticity_substrate_bridge_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for plasticity-substrate bridge
 * WHY:  Easy initialization with biological realism
 * HOW:  Set parameters based on neuroscience measurements
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int plasticity_substrate_default_config(plasticity_substrate_config_t* config);

/**
 * @brief Create plasticity-substrate bridge
 *
 * WHAT: Initialize bridge between plasticity and substrate
 * WHY:  Enable substrate modulation of learning
 * HOW:  Allocate structure, connect to substrate
 *
 * @param config Configuration (NULL for defaults)
 * @param substrate Neural substrate
 * @return New bridge or NULL on failure
 */
plasticity_substrate_bridge_t* plasticity_substrate_bridge_create(
    const plasticity_substrate_config_t* config,
    neural_substrate_t* substrate
);

/**
 * @brief Destroy plasticity-substrate bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure, disconnect bio-async
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void plasticity_substrate_bridge_destroy(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Connect plasticity contexts to bridge
 *
 * WHAT: Optionally provide plasticity mechanism contexts for direct access
 * WHY:  Enable bridge to directly modulate plasticity parameters
 * HOW:  Store opaque pointers (bridge doesn't own these)
 *
 * @param bridge Plasticity substrate bridge
 * @param stdp_ctx STDP context (optional, can be NULL)
 * @param bcm_ctx BCM context (optional, can be NULL)
 * @param homeostatic_ctrl Homeostatic controller (optional, can be NULL)
 * @param eligibility_ctx Eligibility context (optional, can be NULL)
 * @param dendritic_tree Dendritic tree (optional, can be NULL)
 * @return 0 on success
 */
int plasticity_substrate_connect_contexts(
    plasticity_substrate_bridge_t* bridge,
    void* stdp_ctx,
    void* bcm_ctx,
    homeostatic_controller_t homeostatic_ctrl,
    void* eligibility_ctx,
    dendritic_tree_t dendritic_tree
);

/* ============================================================================
 * Bio-async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register bridge with bio-async messaging
 * WHY:  Enable inter-module communication
 * HOW:  Register module context with router
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success, -1 on error
 */
int plasticity_substrate_connect_bio_async(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async
 * WHY:  Clean shutdown
 * HOW:  Deregister module context
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success, -1 on error
 */
int plasticity_substrate_disconnect_bio_async(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Plasticity substrate bridge
 * @return true if connected
 */
bool plasticity_substrate_is_bio_async_connected(const plasticity_substrate_bridge_t* bridge);

/* ============================================================================
 * Update API - Compute Substrate Effects
 * ============================================================================ */

/**
 * @brief Update all plasticity-substrate effects
 *
 * WHAT: Recompute all substrate modulation effects
 * WHY:  Substrate state changes → plasticity parameter changes
 * HOW:  Query substrate state, compute Q10 factors, ATP gating, etc.
 *
 * BIOLOGICAL: Integrates multiple substrate→plasticity pathways
 * COMPLEXITY: O(1) - just computation, no iteration
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_all(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Update STDP modulation effects
 *
 * WHAT: Compute substrate effects on STDP
 * WHY:  Temperature affects STDP window, ATP gates LTP
 * HOW:  Q10 scaling for τ_plus/minus, ATP-based LR gating
 *
 * BIOLOGICAL:
 * - Temperature: τ(T) = τ(37°C) × Q10^((T-37)/10)
 * - ATP gating: LR = base_LR × min(1, ATP / ATP_threshold)
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_stdp(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Update BCM modulation effects
 *
 * WHAT: Compute substrate effects on BCM
 * WHY:  Metabolic stress shifts BCM threshold toward LTD
 * HOW:  Compute threshold shift based on ATP/O2/glucose
 *
 * BIOLOGICAL:
 * - Low energy → higher threshold → bias toward LTD (protective)
 * - Hypoxia → impaired calcium signaling → reduced LTP
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_bcm(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Update homeostatic modulation effects
 *
 * WHAT: Compute substrate effects on homeostatic plasticity
 * WHY:  Substrate degradation triggers compensatory scaling
 * HOW:  Adjust target rates, boost recovery for degraded substrate
 *
 * BIOLOGICAL:
 * - Substrate degradation → lower target firing rate (stability)
 * - Poor health → enhanced homeostatic compensation
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_homeostatic(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Update eligibility trace modulation effects
 *
 * WHAT: Compute substrate effects on eligibility traces
 * WHY:  ATP required for trace maintenance and consolidation
 * HOW:  Modulate decay rate, gate consolidation by ATP
 *
 * BIOLOGICAL:
 * - Trace persistence requires active maintenance (ATP cost)
 * - Consolidation (trace→weight) requires protein synthesis (ATP-dependent)
 * - Energy depletion → faster trace decay
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_eligibility(plasticity_substrate_bridge_t* bridge);

/**
 * @brief Update dendritic modulation effects
 *
 * WHAT: Compute substrate effects on dendritic plasticity
 * WHY:  Membrane integrity and ion balance affect NMDA function
 * HOW:  Gate NMDA conductance, shift spike threshold
 *
 * BIOLOGICAL:
 * - NMDA requires healthy membrane potential for Mg²⁺ unblock
 * - Ionic imbalance impairs dendritic spike generation
 * - Membrane damage reduces Ca²⁺ signaling
 *
 * @param bridge Plasticity substrate bridge
 * @return 0 on success
 */
int plasticity_substrate_update_dendritic(plasticity_substrate_bridge_t* bridge);

/* ============================================================================
 * Query API - Get Modulation Factors
 * ============================================================================ */

/**
 * @brief Get global learning rate modulation
 *
 * WHAT: Get combined LR multiplier from all substrate factors
 * WHY:  Single value for global LR scaling
 * HOW:  Combine ATP, temperature, membrane effects
 *
 * USAGE: effective_LR = base_LR × get_learning_rate_mod(bridge)
 *
 * @param bridge Plasticity substrate bridge
 * @return Learning rate multiplier [0.1-1.5]
 */
float plasticity_substrate_get_learning_rate_mod(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get STDP time window modulation
 *
 * WHAT: Get temperature-based STDP window scaling
 * WHY:  Temperature affects NMDA kinetics → STDP window
 * HOW:  Q10-based scaling
 *
 * USAGE: effective_tau = base_tau × get_stdp_window_mod(bridge)
 *
 * @param bridge Plasticity substrate bridge
 * @return STDP window multiplier [0.5-2.0]
 */
float plasticity_substrate_get_stdp_window_mod(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get BCM threshold shift
 *
 * WHAT: Get metabolic stress effect on BCM threshold
 * WHY:  Stress shifts threshold toward LTD
 * HOW:  Compute from ATP/O2/glucose levels
 *
 * USAGE: effective_theta = base_theta × get_bcm_threshold_shift(bridge)
 *
 * @param bridge Plasticity substrate bridge
 * @return BCM threshold multiplier [1.0-1.5]
 */
float plasticity_substrate_get_bcm_threshold_shift(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get homeostatic target rate adjustment
 *
 * WHAT: Get substrate-based adjustment to target firing rate
 * WHY:  Degraded substrate → lower safe target
 * HOW:  Scale by substrate health
 *
 * @param bridge Plasticity substrate bridge
 * @return Target rate adjustment [0.7-1.0]
 */
float plasticity_substrate_get_homeostatic_adjustment(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get eligibility trace decay modulation
 *
 * WHAT: Get ATP-based modulation of trace decay
 * WHY:  Low ATP → faster decay (can't maintain)
 * HOW:  Scale decay lambda by ATP
 *
 * USAGE: effective_lambda = base_lambda × get_eligibility_decay_mod(bridge)
 *
 * @param bridge Plasticity substrate bridge
 * @return Decay multiplier [0.8-1.2]
 */
float plasticity_substrate_get_eligibility_decay_mod(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get dendritic NMDA conductance modulation
 *
 * WHAT: Get membrane integrity effect on NMDA
 * WHY:  Damaged membrane impairs NMDA function
 * HOW:  Gate by membrane integrity
 *
 * @param bridge Plasticity substrate bridge
 * @return NMDA conductance multiplier [0-1]
 */
float plasticity_substrate_get_nmda_conductance_mod(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get complete substrate effects
 *
 * WHAT: Get full effects structure
 * WHY:  Access all modulation factors at once
 * HOW:  Copy internal effects
 *
 * @param bridge Plasticity substrate bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int plasticity_substrate_get_effects(
    const plasticity_substrate_bridge_t* bridge,
    plasticity_substrate_effects_t* effects
);

/**
 * @brief Check if plasticity is substrate-limited
 *
 * WHAT: Determine if substrate is limiting plasticity
 * WHY:  Know when plasticity is being constrained
 * HOW:  Check if any factor is significantly < 1.0
 *
 * @param bridge Plasticity substrate bridge
 * @return true if limited (LR < 0.8 or blocks active)
 */
bool plasticity_substrate_is_limited(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get overall plasticity capacity
 *
 * WHAT: Get combined plasticity capacity score
 * WHY:  Single metric for substrate health → plasticity
 * HOW:  Combine metabolic and physical capacity
 *
 * @param bridge Plasticity substrate bridge
 * @return Plasticity capacity [0-1]
 */
float plasticity_substrate_get_capacity(const plasticity_substrate_bridge_t* bridge);

/**
 * @brief Get bridge statistics
 *
 * WHAT: Retrieve monitoring metrics
 * WHY:  Track substrate effects on learning
 * HOW:  Copy stats structure
 *
 * @param bridge Plasticity substrate bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int plasticity_substrate_get_stats(
    const plasticity_substrate_bridge_t* bridge,
    plasticity_substrate_stats_t* stats
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PLASTICITY_SUBSTRATE_BRIDGE_H */
