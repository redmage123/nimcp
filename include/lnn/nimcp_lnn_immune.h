/**
 * @file nimcp_lnn_immune.h
 * @brief Immune system integration for LNN networks
 * @version 1.0.0
 * @date 2025-12-20
 *
 * WHAT: Bidirectional immune-LNN integration
 * WHY:  LNN instabilities are threats; immune modulates LNN dynamics
 * HOW:  Report instabilities as antigens; apply cytokine effects to τ
 *
 * BIOLOGICAL BASIS:
 * =================
 * During inflammation/illness:
 * - Neural dynamics slow down to conserve energy (increased τ)
 * - Learning rate suppressed (synaptic plasticity reduced)
 * - Gradient processing stabilized (numerical protection)
 * - State dynamics damped (prevent runaway activation)
 *
 * LNN-specific instabilities:
 * - NaN/Inf in state → acute immune response
 * - State explosion → regional inflammation
 * - Tau explosion/collapse → time constant regulation failure
 * - ODE divergence → numerical instability threat
 * - Gradient explosion/vanishing → learning failure
 *
 * ARCHITECTURE:
 * =============
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    LNN IMMUNE BRIDGE                             │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  LNN → IMMUNE (Instabilities as antigens)                       │
 * │  ┌────────────────┐           ┌─────────────────────────┐      │
 * │  │ LNN Network    │           │   Brain Immune          │      │
 * │  │ State Checks   │──detect──→│   Antigen               │      │
 * │  │ τ Monitoring   │  threat   │   Presentation          │      │
 * │  └────────────────┘           └─────────────────────────┘      │
 * │         │                                                        │
 * │         │ NaN/Inf       → SEVERITY 10 (acute)                   │
 * │         │ State exp     → SEVERITY 8                            │
 * │         │ τ explosion   → SEVERITY 6                            │
 * │         │ Grad exp      → SEVERITY 6                            │
 * │         │ ODE diverge   → SEVERITY 7                            │
 * │                                                                  │
 * │  IMMUNE → LNN (Cytokines modulate dynamics)                     │
 * │  ┌────────────────┐           ┌─────────────────────────┐      │
 * │  │ Brain Immune   │           │   LNN Dynamics          │      │
 * │  │ Inflammation   │──fever──→│   τ scaling             │      │
 * │  │ Cytokines      │  effects  │   LR modulation         │      │
 * │  └────────────────┘           └─────────────────────────┘      │
 * │         │                              │                        │
 * │         │ NONE      → τ × 1.0          │                        │
 * │         │ LOCAL     → τ × 1.05         │                        │
 * │         │ REGIONAL  → τ × 1.15         │                        │
 * │         │ SYSTEMIC  → τ × 1.3          │                        │
 * │         │ STORM     → τ × 1.5 (slow)   │                        │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * NIMCP STANDARDS:
 * ================
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe via mutex
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_LNN_IMMUNE_H
#define NIMCP_LNN_IMMUNE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Core dependencies */
#include "lnn/nimcp_lnn_types.h"
#include "cognitive/immune/nimcp_brain_immune.h"
#include "middleware/immune/nimcp_training_immune.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/logging/nimcp_logging.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LNN_IMMUNE_MODULE_NAME           "lnn_immune"
#define LNN_IMMUNE_MAX_INSTABILITY_HISTORY  64  /**< Max instability event history */

/* Default thresholds */
#define LNN_IMMUNE_STATE_EXPLOSION_DEFAULT      1e6f
#define LNN_IMMUNE_STATE_COLLAPSE_DEFAULT       1e-10f
#define LNN_IMMUNE_TAU_MAX_DEFAULT              1000.0f
#define LNN_IMMUNE_TAU_MIN_DEFAULT              0.1f
#define LNN_IMMUNE_GRAD_EXPLOSION_DEFAULT       1e3f
#define LNN_IMMUNE_GRAD_VANISHING_DEFAULT       1e-7f

/* Default severities per instability type */
#define LNN_IMMUNE_SEVERITY_NAN_STATE           10
#define LNN_IMMUNE_SEVERITY_INF_STATE           10
#define LNN_IMMUNE_SEVERITY_STATE_EXPLOSION     8
#define LNN_IMMUNE_SEVERITY_STATE_COLLAPSE      4
#define LNN_IMMUNE_SEVERITY_TAU_EXPLOSION       6
#define LNN_IMMUNE_SEVERITY_TAU_COLLAPSE        5
#define LNN_IMMUNE_SEVERITY_GRAD_EXPLOSION      6
#define LNN_IMMUNE_SEVERITY_GRAD_VANISHING      4
#define LNN_IMMUNE_SEVERITY_ODE_DIVERGENCE      7

/* Inflammation -> tau scaling mapping (slow down = higher tau) */
#define LNN_IMMUNE_TAU_SCALE_NONE               1.0f
#define LNN_IMMUNE_TAU_SCALE_LOCAL              1.05f
#define LNN_IMMUNE_TAU_SCALE_REGIONAL           1.15f
#define LNN_IMMUNE_TAU_SCALE_SYSTEMIC           1.3f
#define LNN_IMMUNE_TAU_SCALE_STORM              1.5f

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct lnn_immune_bridge lnn_immune_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief LNN instability types (map to immune antigens)
 *
 * BIOLOGICAL BASIS:
 * Different neural failures map to different immune threat types
 */
typedef enum {
    LNN_INSTABILITY_NONE = 0,           /**< No instability detected */
    LNN_INSTABILITY_NAN_STATE,          /**< NaN in state vector */
    LNN_INSTABILITY_INF_STATE,          /**< Inf in state vector */
    LNN_INSTABILITY_STATE_EXPLOSION,    /**< ||x|| > threshold */
    LNN_INSTABILITY_STATE_COLLAPSE,     /**< ||x|| < threshold */
    LNN_INSTABILITY_TAU_EXPLOSION,      /**< τ > max_tau */
    LNN_INSTABILITY_TAU_COLLAPSE,       /**< τ < min_tau */
    LNN_INSTABILITY_GRADIENT_EXPLOSION, /**< ||∇|| > threshold */
    LNN_INSTABILITY_GRADIENT_VANISHING, /**< ||∇|| < threshold */
    LNN_INSTABILITY_ODE_DIVERGENCE,     /**< ODE solver diverged */
    LNN_INSTABILITY_COUNT
} lnn_instability_type_t;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

/**
 * @brief Cytokine effects on LNN dynamics
 *
 * WHAT: How immune inflammation modulates LNN behavior
 * WHY:  Fever model - slow down dynamics, reduce learning during illness
 * HOW:  Apply scaling factors to τ, LR, state updates
 */
typedef struct {
    /* τ modulation (fever model: higher inflammation → slower dynamics) */
    float tau_scale;            /**< Scale factor for time constants (>1 = slower) */
    float tau_offset;           /**< Additive offset to time constants */

    /* Learning modulation (reduce learning during inflammation) */
    float lr_factor;            /**< Learning rate multiplier (<1 = slower learning) */
    float gradient_scale;       /**< Gradient scaling factor */

    /* State modulation (damping and exploration) */
    float state_damping;        /**< Damping factor for state updates (<1 = damped) */
    float noise_injection;      /**< Noise level for exploration (exploration vs exploitation) */

    /* Inflammation level that generated these effects */
    brain_inflammation_level_t inflammation;

    /* Validity flag */
    bool valid;                 /**< Whether effects are valid */
} lnn_cytokine_effects_t;

/**
 * @brief LNN immune bridge configuration
 */
typedef struct {
    /* Instability detection thresholds */
    float state_explosion_threshold;    /**< State norm threshold for explosion */
    float state_collapse_threshold;     /**< State norm threshold for collapse */
    float tau_max;                      /**< Maximum allowed τ */
    float tau_min;                      /**< Minimum allowed τ */
    float gradient_explosion_threshold; /**< Gradient norm for explosion */
    float gradient_vanishing_threshold; /**< Gradient norm for vanishing */

    /* Immune response configuration */
    bool auto_report_instabilities;     /**< Auto-report instabilities as antigens */
    uint8_t instability_severity[LNN_INSTABILITY_COUNT]; /**< Severity per type */

    /* Cytokine modulation settings */
    bool enable_tau_modulation;         /**< Enable τ modulation */
    bool enable_lr_modulation;          /**< Enable LR modulation */
    bool enable_state_damping;          /**< Enable state damping */

    /* Custom inflammation -> effect mappings */
    float tau_inflammation_scales[5];   /**< τ scale per inflammation level [NONE..STORM] */
    float lr_inflammation_factors[5];   /**< LR factor per inflammation level */

    /* Bio-async */
    bool enable_bio_async;              /**< Enable bio-async messaging */
} lnn_immune_config_t;

/**
 * @brief LNN immune bridge statistics
 */
typedef struct {
    /* Instability detections */
    uint64_t total_instabilities;
    uint64_t instabilities_by_type[LNN_INSTABILITY_COUNT];

    /* Immune responses */
    uint64_t antigens_presented;
    uint64_t immune_responses_active;

    /* Effect applications */
    uint64_t tau_modulations;
    uint64_t lr_modulations;
    uint64_t state_dampings;

    /* Current state */
    brain_inflammation_level_t current_inflammation;
    float current_tau_scale;
    float current_lr_factor;

    /* Health */
    float avg_tau_modulation;
    float avg_lr_reduction;
} lnn_immune_stats_t;

/**
 * @brief LNN immune bridge
 *
 * WHAT: Coordination layer between LNN network and immune systems
 * WHY:  Bidirectional integration for stability and modulation
 */
struct lnn_immune_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* LNN network */
    lnn_network_t* network;

    /* Immune systems */
    brain_immune_system_t* brain_immune;
    training_immune_system_t* training_immune;

    /* Configuration */
    lnn_immune_config_t config;

    /* Current cytokine effects */
    lnn_cytokine_effects_t cytokine_effects;

    /* Statistics */
    lnn_immune_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with good defaults
 * HOW:  Return struct with balanced parameters
 *
 * @param config Output configuration
 * @return 0 on success, negative on error
 */
int lnn_immune_config_default(lnn_immune_config_t* config);

/**
 * @brief Create LNN immune bridge
 *
 * WHAT: Initialize immune integration for LNN network
 * WHY:  Enable bidirectional immune-LNN coordination
 * HOW:  Allocate bridge, set up connections
 *
 * @param network LNN network to integrate
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
lnn_immune_bridge_t* lnn_immune_bridge_create(
    lnn_network_t* network,
    const lnn_immune_config_t* config
);

/**
 * @brief Destroy LNN immune bridge
 *
 * WHAT: Clean up immune bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free buffers, disconnect from immune systems
 *
 * @param bridge Bridge to destroy
 */
void lnn_immune_bridge_destroy(lnn_immune_bridge_t* bridge);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to brain immune system
 *
 * WHAT: Link bridge to brain immune for inflammation state
 * WHY:  Receive cytokine effects to modulate LNN dynamics
 * HOW:  Store handle, register for inflammation updates
 *
 * @param bridge LNN immune bridge
 * @param brain_immune Brain immune system
 * @return 0 on success
 */
int lnn_immune_connect_brain_immune(
    lnn_immune_bridge_t* bridge,
    brain_immune_system_t* brain_immune
);

/**
 * @brief Connect to training immune system
 *
 * WHAT: Link bridge to training immune for training coordination
 * WHY:  Share LNN instabilities with training monitoring
 * HOW:  Store handle, coordinate instability reporting
 *
 * @param bridge LNN immune bridge
 * @param training_immune Training immune system
 * @return 0 on success
 */
int lnn_immune_connect_training_immune(
    lnn_immune_bridge_t* bridge,
    training_immune_system_t* training_immune
);

/**
 * @brief Connect immune bridge to LNN network
 *
 * WHAT: Register immune bridge with LNN network for bidirectional integration
 * WHY:  Enable network to receive immune modulation and report instabilities
 * HOW:  Store bridge reference in network, enable callbacks
 *
 * @param network LNN network to connect
 * @param immune_bridge Immune bridge instance (may be NULL to disconnect)
 * @return 0 on success, negative on error
 */
int lnn_immune_connect(lnn_network_t* network, void* immune_bridge);

/* ============================================================================
 * Stability Monitoring API (LNN → Immune)
 * ============================================================================ */

/**
 * @brief Check LNN state for instabilities
 *
 * WHAT: Scan network state for instabilities
 * WHY:  Early detection enables immune response
 * HOW:  Check state norms, τ bounds, gradient health
 *
 * @param bridge LNN immune bridge
 * @return Detected instability type (LNN_INSTABILITY_NONE if healthy)
 */
lnn_instability_type_t lnn_immune_check_stability(
    lnn_immune_bridge_t* bridge
);

/**
 * @brief Report instability to immune system
 *
 * WHAT: Present LNN instability as immune antigen
 * WHY:  Trigger immune response to LNN failure
 * HOW:  Create epitope from instability, present to brain immune
 *
 * @param bridge LNN immune bridge
 * @param type Instability type
 * @param layer_id Layer where instability occurred (0 if network-wide)
 * @param neuron_id Neuron ID where instability occurred (0 if layer-wide)
 * @return 0 on success
 */
int lnn_immune_report_instability(
    lnn_immune_bridge_t* bridge,
    lnn_instability_type_t type,
    uint32_t layer_id,
    uint32_t neuron_id
);

/* ============================================================================
 * Cytokine Effect Management API (Immune → LNN)
 * ============================================================================ */

/**
 * @brief Update cytokine effects from immune system
 *
 * WHAT: Receive current immune state and compute LNN effects
 * WHY:  Translate inflammation to LNN modulation
 * HOW:  Query brain immune, compute τ/LR/damping factors
 *
 * @param bridge LNN immune bridge
 * @return 0 on success
 */
int lnn_immune_update_effects(lnn_immune_bridge_t* bridge);

/**
 * @brief Apply cytokine effects to LNN dynamics
 *
 * WHAT: Modify LNN time constants, learning rates based on immune state
 * WHY:  Implement fever model - slow down during inflammation
 * HOW:  Scale τ, reduce LR, apply damping to network
 *
 * @param bridge LNN immune bridge
 * @return 0 on success
 */
int lnn_immune_apply_effects(lnn_immune_bridge_t* bridge);

/**
 * @brief Get current cytokine effects
 *
 * WHAT: Retrieve current immune modulation parameters
 * WHY:  Allow external inspection of immune influence
 * HOW:  Copy effects structure
 *
 * @param bridge LNN immune bridge
 * @param effects Output: current effects
 * @return 0 on success
 */
int lnn_immune_get_effects(
    const lnn_immune_bridge_t* bridge,
    lnn_cytokine_effects_t* effects
);

/* ============================================================================
 * Query and Statistics API
 * ============================================================================ */

/**
 * @brief Get statistics
 *
 * @param bridge LNN immune bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int lnn_immune_get_stats(
    const lnn_immune_bridge_t* bridge,
    lnn_immune_stats_t* stats
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Convert instability type to string
 *
 * @param type Instability type
 * @return String representation
 */
const char* lnn_instability_type_to_string(lnn_instability_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_LNN_IMMUNE_H */
