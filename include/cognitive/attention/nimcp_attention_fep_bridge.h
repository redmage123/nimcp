/**
 * @file nimcp_attention_fep_bridge.h
 * @brief Free Energy Principle - Attention Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and attention system
 * WHY:  Attention implements precision-weighting of prediction errors. FEP provides
 *       theoretical grounding: attention = precision optimization.
 * HOW:  FEP precision values modulate attention gain; attention focus influences
 *       FEP belief updates by gating sensory prediction errors.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * ATTENTION AS PRECISION OPTIMIZATION:
 * ------------------------------------
 * - Feldman & Friston (2010): Attention = optimizing precision of predictions
 * - High precision → Increased gain on prediction errors → Stronger attention
 * - Low precision → Reduced gain → Attentional suppression
 * - Dopamine/ACh modulate precision (Friston et al., 2012)
 *
 * FEP → ATTENTION PATHWAYS:
 * -------------------------
 * 1. Precision-Weighted Gain:
 *    - High FEP precision → Boost attention gain
 *    - Low precision → Reduce attention gain
 *    - Precision = confidence in predictions
 *
 * 2. Surprise-Driven Attention Shifts:
 *    - High prediction errors → Attention shift
 *    - Salience = precision-weighted surprise
 *    - Reference: Itti & Baldi (2009) "Bayesian surprise attracts attention"
 *
 * 3. Expected Free Energy Guides Attention:
 *    - Policies minimize EFE
 *    - Attention orients to maximize information gain
 *    - Exploration vs exploitation trade-off
 *
 * ATTENTION → FEP PATHWAYS:
 * -------------------------
 * 1. Attentional Gating of Prediction Errors:
 *    - Attended stimuli → High precision PEs
 *    - Unattended stimuli → Low precision PEs
 *    - Implements selective sensory processing
 *
 * 2. Focus Modulates Learning Rate:
 *    - High attention → Faster belief updates
 *    - Low attention → Slower updates
 *    - Biological: ACh modulates cortical plasticity
 *
 * 3. Attention States Influence Generative Model:
 *    - Focused attention → Narrow generative model
 *    - Diffuse attention → Broad generative model
 *    - Context-dependent belief priors
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                  FEP-ATTENTION BRIDGE                                      ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                FEP → ATTENTION PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ FEP PRECISION│ ──→ Attention Gain Modulation                   │  ║
 * ║   │   │  (High Π)    │     - Precision 10.0 → Gain +50%                │  ║
 * ║   │   │  (Low Π)     │     - Precision 0.1  → Gain -50%                │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ PRED ERRORS  │ ──→ Attention Shift (surprise-driven)           │  ║
 * ║   │   │  PE > 5.0    │     - High PE → Orient attention                │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │ EXP FREE NRG │ ──→ Information-seeking attention               │  ║
 * ║   │   │ G(π)         │     - Min G → Attend to informative stimuli     │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              ATTENTION → FEP PATHWAYS                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ ATTENTION FOCUS  │ ──→ PE Precision Gating                     │  ║
 * ║   │   │  (attended)      │     - Attended → Π × 2.0                    │  ║
 * ║   │   │  (unattended)    │     - Unattended → Π × 0.5                  │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ ATTENTION GAIN   │ ──→ Belief Update Rate                      │  ║
 * ║   │   │  (high gain)     │     - High → LR × 1.5                       │  ║
 * ║   │   │  (low gain)      │     - Low → LR × 0.5                        │  ║
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

#ifndef NIMCP_ATTENTION_FEP_BRIDGE_H
#define NIMCP_ATTENTION_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "plasticity/attention/nimcp_attention.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Precision-gain mapping factors */
#define ATTENTION_FEP_HIGH_PRECISION_GAIN     1.5f   /**< Gain boost for high precision */
#define ATTENTION_FEP_LOW_PRECISION_GAIN      0.5f   /**< Gain reduction for low precision */
#define ATTENTION_FEP_PRECISION_THRESHOLD     1.0f   /**< Precision threshold for gain modulation */

/* Attention-precision gating factors */
#define ATTENTION_FEP_ATTENDED_PRECISION_MULT   2.0f   /**< Precision multiplier for attended */
#define ATTENTION_FEP_UNATTENDED_PRECISION_MULT 0.5f   /**< Precision multiplier for unattended */
#define ATTENTION_FEP_FOCUS_THRESHOLD           0.5f   /**< Attention threshold for gating */

/* Learning rate modulation */
#define ATTENTION_FEP_HIGH_GAIN_LR_MULT       1.5f   /**< LR multiplier for high attention */
#define ATTENTION_FEP_LOW_GAIN_LR_MULT        0.5f   /**< LR multiplier for low attention */

/* Surprise-driven attention */
#define ATTENTION_FEP_SURPRISE_SHIFT_THRESHOLD  5.0f   /**< PE threshold for attention shift */
#define ATTENTION_FEP_EFE_THRESHOLD             3.0f   /**< EFE threshold for info-seeking */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct attention_fep_bridge attention_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Attention-FEP bridge
 */
typedef struct {
    /* FEP → Attention */
    float precision_gain_scaling;        /**< How much precision affects gain */
    float pe_attention_shift_threshold;  /**< PE threshold for attention shift */
    float efe_info_seeking_threshold;    /**< EFE threshold for info-seeking */
    bool enable_precision_gain_modulation; /**< Enable precision → gain */
    bool enable_surprise_attention_shift;  /**< Enable PE → attention shift */
    bool enable_efe_info_seeking;          /**< Enable EFE-guided attention */

    /* Attention → FEP */
    float attended_precision_boost;      /**< Precision boost for attended */
    float unattended_precision_reduction; /**< Precision reduction for unattended */
    float attention_learning_rate_scaling; /**< How much attention affects LR */
    bool enable_attentional_gating;      /**< Enable attention → precision gating */
    bool enable_attention_lr_modulation; /**< Enable attention → LR modulation */
    bool enable_focus_model_narrowing;   /**< Enable focus → model narrowing */

    /* Sensitivity factors */
    float precision_sensitivity;         /**< Precision effect scaling */
    float attention_sensitivity;         /**< Attention effect scaling */
} attention_fep_config_t;

/**
 * @brief FEP effects on attention
 */
typedef struct {
    /* Precision effects */
    float precision_value;               /**< Current FEP precision */
    float precision_gain_modifier;       /**< Gain modifier from precision */

    /* Prediction error effects */
    float current_prediction_error;      /**< Current PE magnitude */
    bool surprise_shift_active;          /**< Surprise-driven shift active */

    /* Expected free energy effects */
    float current_efe;                   /**< Current EFE value */
    bool info_seeking_active;            /**< Info-seeking mode active */

    /* Combined effects */
    float total_gain_modulation;         /**< Combined gain modulation */
} attention_fep_effects_t;

/**
 * @brief Attention effects on FEP
 */
typedef struct {
    /* Attention state */
    float attention_focus;               /**< Current attention focus [0-1] */
    float attention_gain;                /**< Current attention gain */

    /* Precision gating */
    float gated_precision;               /**< Attention-gated precision */
    float precision_multiplier;          /**< Precision gating factor */

    /* Learning rate modulation */
    float learning_rate_modifier;        /**< LR modifier from attention */

    /* Model effects */
    float model_narrowing_factor;        /**< Model narrowing from focus */
} fep_attention_effects_t;

/**
 * @brief Current state of Attention-FEP interaction
 */
typedef struct {
    /* Current values */
    float current_precision;             /**< Current FEP precision */
    float current_attention_focus;       /**< Current attention focus */
    float current_prediction_error;      /**< Current PE magnitude */

    /* Applied modifiers */
    float gain_modulation;               /**< Applied gain modulation */
    float precision_gating;              /**< Applied precision gating */
    float lr_modulation;                 /**< Applied LR modulation */

    /* State flags */
    bool surprise_shift_triggered;       /**< Surprise shift triggered */
    bool info_seeking_active;            /**< Info-seeking active */
    bool attentional_gating_active;      /**< Attentional gating active */

    /* Timestamps */
    uint64_t last_surprise_shift_time;   /**< Last surprise shift timestamp */
    uint64_t last_efe_update_time;       /**< Last EFE update timestamp */
} attention_fep_state_t;

/**
 * @brief Statistics for Attention-FEP bridge
 */
typedef struct {
    /* FEP → Attention */
    uint64_t precision_gain_modulations; /**< Times precision modulated gain */
    uint64_t surprise_attention_shifts;  /**< Surprise-driven attention shifts */
    uint64_t efe_info_seeking_events;    /**< EFE-guided info-seeking events */
    float avg_precision;                 /**< Average FEP precision */
    float avg_gain_modulation;           /**< Average gain modulation */

    /* Attention → FEP */
    uint64_t attentional_gating_events;  /**< Attention gating events */
    uint64_t lr_modulation_events;       /**< LR modulation events */
    uint64_t model_narrowing_events;     /**< Model narrowing events */
    float avg_attention_focus;           /**< Average attention focus */
    float avg_precision_gating;          /**< Average precision gating */

    /* Performance */
    float avg_prediction_error;          /**< Average PE magnitude */
    float avg_free_energy;               /**< Average free energy */
} attention_fep_stats_t;

/**
 * @brief Attention-FEP bridge state
 */
struct attention_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    attention_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    multihead_attention_t attention;     /**< Attention system */

    /* Current effects */
    attention_fep_effects_t fep_effects; /**< FEP → Attention */
    fep_attention_effects_t attention_effects; /**< Attention → FEP */
    attention_fep_state_t state;

    /* Statistics */
    attention_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Attention-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int attention_fep_bridge_default_config(attention_fep_config_t* config);

/**
 * @brief Create Attention-FEP bridge
 *
 * WHAT: Initialize Attention-FEP integration bridge
 * WHY:  Enable bidirectional Attention-FEP interaction
 * HOW:  Allocate bridge, link systems, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
attention_fep_bridge_t* attention_fep_bridge_create(const attention_fep_config_t* config);

/**
 * @brief Destroy Attention-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void attention_fep_bridge_destroy(attention_fep_bridge_t* bridge);

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
 * @param bridge Attention-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int attention_fep_bridge_connect_fep(
    attention_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect attention system
 *
 * WHAT: Link bridge to attention system
 * WHY:  Enable attention state monitoring and modulation
 * HOW:  Store attention system handle
 *
 * @param bridge Attention-FEP bridge
 * @param attention Attention system
 * @return 0 on success
 */
int attention_fep_bridge_connect_attention(
    attention_fep_bridge_t* bridge,
    multihead_attention_t attention
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and attention systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_bridge_disconnect(attention_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → Attention Direction
 * ============================================================================ */

/**
 * @brief Apply precision modulation to attention gain
 *
 * WHAT: Modulate attention gain based on FEP precision
 * WHY:  High precision → high gain (attention = precision optimization)
 * HOW:  Scale attention gain by precision value
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_apply_precision_gain_modulation(attention_fep_bridge_t* bridge);

/**
 * @brief Trigger surprise-driven attention shift
 *
 * WHAT: Shift attention to high prediction error locations
 * WHY:  Surprise attracts attention (Bayesian salience)
 * HOW:  Detect high PE, orient attention
 *
 * @param bridge Attention-FEP bridge
 * @param pe_magnitude Prediction error magnitude
 * @return 0 on success
 */
int attention_fep_surprise_attention_shift(
    attention_fep_bridge_t* bridge,
    float pe_magnitude
);

/**
 * @brief Guide attention via expected free energy
 *
 * WHAT: Orient attention to minimize EFE (info-seeking)
 * WHY:  Attention maximizes information gain
 * HOW:  Evaluate policies, attend to informative stimuli
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_efe_info_seeking(attention_fep_bridge_t* bridge);

/* ============================================================================
 * Attention → FEP Direction
 * ============================================================================ */

/**
 * @brief Apply attentional gating to precision
 *
 * WHAT: Modulate FEP precision based on attention focus
 * WHY:  Attended stimuli have higher precision
 * HOW:  Multiply precision by attention focus factor
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_apply_attentional_gating(attention_fep_bridge_t* bridge);

/**
 * @brief Modulate learning rate by attention gain
 *
 * WHAT: Adjust FEP learning rate based on attention
 * WHY:  High attention → faster belief updates
 * HOW:  Scale learning rate by attention gain
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_modulate_learning_rate(attention_fep_bridge_t* bridge);

/**
 * @brief Narrow generative model based on focus
 *
 * WHAT: Adjust generative model scope by attention focus
 * WHY:  Focused attention → narrow model; diffuse → broad
 * HOW:  Modulate model prior variance by focus
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_apply_focus_model_narrowing(attention_fep_bridge_t* bridge);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Attention-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep Attention and FEP systems synchronized
 * HOW:  Update effects, apply modulations, check thresholds
 *
 * @param bridge Attention-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int attention_fep_bridge_update(
    attention_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Attention-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int attention_fep_bridge_get_state(
    const attention_fep_bridge_t* bridge,
    attention_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Attention-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int attention_fep_bridge_get_stats(
    const attention_fep_bridge_t* bridge,
    attention_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for Attention-FEP coordination
 * WHY:  Distributed attention-precision signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_bridge_connect_bio_async(attention_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Attention-FEP bridge
 * @return 0 on success
 */
int attention_fep_bridge_disconnect_bio_async(attention_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Attention-FEP bridge
 * @return true if bio-async enabled
 */
bool attention_fep_bridge_is_bio_async_connected(
    const attention_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_ATTENTION_FEP_BRIDGE_H */
