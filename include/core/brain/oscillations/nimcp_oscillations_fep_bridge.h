/**
 * @file nimcp_oscillations_fep_bridge.h
 * @brief Free Energy Principle - Oscillations Integration Bridge
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Bidirectional integration between Free Energy Principle and neural oscillations
 * WHY:  Neural oscillations provide temporal structure for predictive processing;
 *       oscillatory phase/frequency encode precision and hierarchical binding. FEP
 *       prediction errors modulate oscillatory dynamics.
 * HOW:  Map FEP precision to oscillatory power/frequency; use gamma for prediction
 *       error, beta for predictions, theta for hierarchical integration; cross-frequency
 *       coupling reflects hierarchical belief propagation.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * OSCILLATIONS AS FEP TEMPORAL STRUCTURE:
 * ---------------------------------------
 * 1. Gamma Oscillations (30-80 Hz) - Prediction Errors:
 *    - High gamma power = high prediction error
 *    - Bottom-up sensory processing
 *    - Superficial layer (L2/3) generation
 *    - Reference: Bastos et al. (2015) "Visual areas exert feedforward and feedback
 *      influences through distinct frequency channels"
 *
 * 2. Beta Oscillations (13-30 Hz) - Top-Down Predictions:
 *    - High beta power = strong predictions
 *    - Top-down modulatory signals
 *    - Deep layer (L5/6) generation
 *    - Reference: Bastos et al. (2012) "Canonical microcircuits for predictive coding"
 *
 * 3. Alpha Oscillations (8-12 Hz) - Precision Gating:
 *    - Alpha power inversely related to precision
 *    - Attention modulates alpha
 *    - Inhibitory gating mechanism
 *    - Reference: Feldman & Friston (2010) "Attention, uncertainty, and free-energy"
 *
 * 4. Theta Oscillations (4-8 Hz) - Hierarchical Integration:
 *    - Cross-frequency coupling (theta-gamma PAC)
 *    - Hierarchical belief binding
 *    - Temporal context windows
 *    - Reference: Lisman & Jensen (2013) "The theta-gamma neural code"
 *
 * 5. Delta Oscillations (1-4 Hz) - Slow Belief Updates:
 *    - Slow hierarchical dynamics
 *    - Context-level predictions
 *    - Deep sleep consolidation
 *    - Reference: Steriade (2006) "Grouping of brain rhythms"
 *
 * PRECISION-WEIGHTED PREDICTION ERRORS IN OSCILLATIONS:
 * ----------------------------------------------------
 * - High precision → Strong gamma oscillations
 * - Low precision → Strong alpha oscillations (inhibition)
 * - Precision = Γ (gamma amplitude) / α (alpha amplitude)
 * - Reference: Auksztulewicz & Friston (2016) "Repetition suppression and its
 *   contextual determinants in predictive coding"
 *
 * FEP → OSCILLATIONS PATHWAYS:
 * ---------------------------
 * 1. Prediction Error → Gamma Power:
 *    - FEP prediction error magnitude → Gamma amplitude
 *    - High PE → Increased gamma synchrony
 *    - Drives bottom-up attention
 *
 * 2. Prediction Strength → Beta Power:
 *    - FEP top-down predictions → Beta amplitude
 *    - Strong priors → Increased beta
 *    - Suppresses prediction errors
 *
 * 3. Precision → Alpha/Gamma Ratio:
 *    - High precision → Low alpha, high gamma
 *    - Low precision → High alpha (gating)
 *    - Attention modulates precision
 *
 * OSCILLATIONS → FEP PATHWAYS:
 * ---------------------------
 * 1. Gamma Power → Prediction Error Weighting:
 *    - Gamma amplitude → Precision weight
 *    - Cross-regional gamma coherence → Belief binding
 *    - Gamma phase → Temporal prediction alignment
 *
 * 2. Theta-Gamma PAC → Hierarchical Precision:
 *    - PAC strength → Hierarchical integration
 *    - Theta phase → Context window
 *    - Gamma bursts at theta peaks → Precision-weighted updates
 *
 * 3. Beta Power → Prior Strength:
 *    - Beta amplitude → Top-down prediction gain
 *    - Beta coherence → Cross-regional prediction propagation
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP-OSCILLATIONS BRIDGE                                 ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP → OSCILLATIONS                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ PREDICTION ERROR │                                             │  ║
 * ║   │   │ ──────────────── │                                             │  ║
 * ║   │   │ ε magnitude      │ ───→ Gamma Power (30-80 Hz)                 │  ║
 * ║   │   │ ε precision Π    │ ───→ Gamma/Alpha Ratio                      │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ PREDICTIONS μ̂    │ ───→ Beta Power (13-30 Hz)                  │  ║
 * ║   │   │ PRECISION Π      │ ───→ Alpha Suppression (8-12 Hz)            │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ HIERARCHY LEVEL  │ ───→ Theta-Gamma PAC (4-8 Hz / 30-80 Hz)    │  ║
 * ║   │   │ FREE ENERGY F    │ ───→ Delta Modulation (1-4 Hz)              │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                OSCILLATIONS → FEP                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ GAMMA POWER      │ ───→ Prediction Error Precision             │  ║
 * ║   │   │ GAMMA PHASE      │ ───→ Temporal Alignment                     │  ║
 * ║   │   │ GAMMA COHERENCE  │ ───→ Belief Binding                         │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ BETA POWER       │ ───→ Prior Strength                         │  ║
 * ║   │   │ ALPHA POWER      │ ───→ Precision Inhibition                   │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ THETA-GAMMA PAC  │ ───→ Hierarchical Integration               │  ║
 * ║   │   │ CROSS-FREQUENCY  │ ───→ Multi-level Belief Propagation         │  ║
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

#ifndef NIMCP_OSCILLATIONS_FEP_BRIDGE_H
#define NIMCP_OSCILLATIONS_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "core/brain/oscillations/nimcp_brain_complex_oscillations.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Oscillation band frequency ranges (Hz) */
#define OSC_FEP_DELTA_MIN         1.0f    /**< Delta min frequency */
#define OSC_FEP_DELTA_MAX         4.0f    /**< Delta max frequency */
#define OSC_FEP_THETA_MIN         4.0f    /**< Theta min frequency */
#define OSC_FEP_THETA_MAX         8.0f    /**< Theta max frequency */
#define OSC_FEP_ALPHA_MIN         8.0f    /**< Alpha min frequency */
#define OSC_FEP_ALPHA_MAX        12.0f    /**< Alpha max frequency */
#define OSC_FEP_BETA_MIN         13.0f    /**< Beta min frequency */
#define OSC_FEP_BETA_MAX         30.0f    /**< Beta max frequency */
#define OSC_FEP_GAMMA_MIN        30.0f    /**< Gamma min frequency */
#define OSC_FEP_GAMMA_MAX        80.0f    /**< Gamma max frequency */

/* Coupling constants */
#define OSC_FEP_PE_GAMMA_COUPLING        1.5f   /**< PE → gamma power gain */
#define OSC_FEP_PREDICTION_BETA_COUPLING 1.2f   /**< Prediction → beta power gain */
#define OSC_FEP_PRECISION_ALPHA_COUPLING 0.8f   /**< Precision → alpha suppression */
#define OSC_FEP_HIERARCHY_THETA_COUPLING 1.0f   /**< Hierarchy → theta modulation */

/* PAC thresholds */
#define OSC_FEP_PAC_THRESHOLD            0.3f   /**< Significant PAC threshold */
#define OSC_FEP_COHERENCE_THRESHOLD      0.5f   /**< Significant coherence */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct oscillations_fep_bridge oscillations_fep_bridge_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Oscillatory band types
 */
typedef enum {
    OSC_BAND_DELTA = 0,      /**< Delta (1-4 Hz) - Slow belief updates */
    OSC_BAND_THETA,          /**< Theta (4-8 Hz) - Hierarchical integration */
    OSC_BAND_ALPHA,          /**< Alpha (8-12 Hz) - Precision gating */
    OSC_BAND_BETA,           /**< Beta (13-30 Hz) - Top-down predictions */
    OSC_BAND_GAMMA,          /**< Gamma (30-80 Hz) - Prediction errors */
    OSC_BAND_COUNT
} oscillation_band_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for Oscillations-FEP bridge
 */
typedef struct {
    /* Coupling enables */
    bool enable_pe_gamma_coupling;         /**< PE → gamma coupling */
    bool enable_prediction_beta_coupling;  /**< Prediction → beta coupling */
    bool enable_precision_alpha_coupling;  /**< Precision → alpha coupling */
    bool enable_theta_gamma_pac;           /**< Theta-gamma PAC */

    /* Coupling strengths */
    float pe_gamma_gain;                   /**< PE → gamma gain */
    float prediction_beta_gain;            /**< Prediction → beta gain */
    float precision_alpha_gain;            /**< Precision → alpha gain */
    float hierarchy_theta_gain;            /**< Hierarchy → theta gain */

    /* Thresholds */
    float pac_threshold;                   /**< PAC significance threshold */
    float coherence_threshold;             /**< Coherence significance threshold */

    /* Learning rates */
    float power_adaptation_rate;           /**< Oscillatory power adaptation */
    float phase_adaptation_rate;           /**< Phase alignment rate */
} oscillations_fep_config_t;

/**
 * @brief Oscillatory band power
 */
typedef struct {
    float delta;                           /**< Delta band power */
    float theta;                           /**< Theta band power */
    float alpha;                           /**< Alpha band power */
    float beta;                            /**< Beta band power */
    float gamma;                           /**< Gamma band power */
} oscillation_band_power_t;

/**
 * @brief Effects of FEP on oscillations
 */
typedef struct {
    /* Power modulation */
    float gamma_modulation;                /**< PE → gamma power */
    float beta_modulation;                 /**< Prediction → beta power */
    float alpha_modulation;                /**< Precision → alpha power */
    float theta_modulation;                /**< Hierarchy → theta power */

    /* Phase modulation */
    float gamma_phase_shift;               /**< PE timing → gamma phase */
    float beta_phase_shift;                /**< Prediction timing → beta phase */

    /* Cross-frequency coupling */
    float theta_gamma_pac;                 /**< Theta-gamma PAC strength */
    float alpha_beta_coupling;             /**< Alpha-beta coupling */
} oscillations_fep_effects_t;

/**
 * @brief Current state of Oscillations-FEP interaction
 */
typedef struct {
    /* Current band power */
    oscillation_band_power_t band_power;

    /* FEP-derived oscillatory state */
    float current_gamma;                   /**< Current gamma power */
    float current_beta;                    /**< Current beta power */
    float current_alpha;                   /**< Current alpha power */
    float current_theta;                   /**< Current theta power */

    /* Precision encoding */
    float gamma_alpha_ratio;               /**< Precision = γ/α */
    float effective_precision;             /**< Oscillation-derived precision */

    /* PAC state */
    float theta_gamma_pac_strength;        /**< Current PAC strength */
    float pac_preferred_phase;             /**< Preferred theta phase */

    /* Coherence */
    float cross_regional_coherence;        /**< Inter-regional phase coherence */
} oscillations_fep_state_t;

/**
 * @brief Statistics for Oscillations-FEP bridge
 */
typedef struct {
    /* Modulation events */
    uint64_t gamma_modulations;            /**< Times PE modulated gamma */
    uint64_t beta_modulations;             /**< Times predictions modulated beta */
    uint64_t alpha_modulations;            /**< Times precision modulated alpha */
    uint64_t pac_detections;               /**< Significant PAC events */

    /* Average power */
    float avg_gamma_power;                 /**< Average gamma power */
    float avg_beta_power;                  /**< Average beta power */
    float avg_alpha_power;                 /**< Average alpha power */
    float avg_theta_power;                 /**< Average theta power */

    /* Coupling metrics */
    float avg_theta_gamma_pac;             /**< Average PAC strength */
    float avg_gamma_alpha_ratio;           /**< Average precision ratio */
    float avg_coherence;                   /**< Average phase coherence */
} oscillations_fep_stats_t;

/**
 * @brief Oscillations-FEP bridge state
 */
struct oscillations_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    oscillations_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;                      /**< FEP system */
    brain_complex_oscillation_state_t* osc_state;  /**< Oscillation state */

    /* Current effects */
    oscillations_fep_effects_t effects;
    oscillations_fep_state_t state;

    /* Statistics */
    oscillations_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default Oscillations-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard coupling parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int oscillations_fep_bridge_default_config(
    oscillations_fep_config_t* config
);

/**
 * @brief Create Oscillations-FEP bridge
 *
 * WHAT: Initialize Oscillations-FEP integration bridge
 * WHY:  Enable bidirectional FEP-oscillations interaction
 * HOW:  Allocate bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
oscillations_fep_bridge_t* oscillations_fep_bridge_create(
    const oscillations_fep_config_t* config
);

/**
 * @brief Destroy Oscillations-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free mutex, disconnect systems
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void oscillations_fep_bridge_destroy(
    oscillations_fep_bridge_t* bridge
);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state access
 * HOW:  Store FEP system pointer
 *
 * @param bridge Oscillations-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int oscillations_fep_bridge_connect_fep(
    oscillations_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect oscillation state
 *
 * WHAT: Link bridge to oscillation state
 * WHY:  Enable oscillatory power/phase access
 * HOW:  Store oscillation state pointer
 *
 * @param bridge Oscillations-FEP bridge
 * @param osc_state Oscillation state
 * @return 0 on success
 */
int oscillations_fep_bridge_connect_oscillations(
    oscillations_fep_bridge_t* bridge,
    brain_complex_oscillation_state_t* osc_state
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and oscillations
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_bridge_disconnect(
    oscillations_fep_bridge_t* bridge
);

/* ============================================================================
 * FEP → Oscillations Direction
 * ============================================================================ */

/**
 * @brief Modulate gamma power from prediction error
 *
 * WHAT: Increase gamma power based on FEP prediction error
 * WHY:  Gamma encodes bottom-up surprise
 * HOW:  Gamma power ∝ PE magnitude × precision
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_modulate_gamma_from_pe(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Modulate beta power from predictions
 *
 * WHAT: Increase beta power based on FEP top-down predictions
 * WHY:  Beta encodes top-down priors
 * HOW:  Beta power ∝ prediction strength
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_modulate_beta_from_predictions(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Modulate alpha power from precision
 *
 * WHAT: Suppress alpha based on FEP precision
 * WHY:  Alpha inversely related to precision (gating)
 * HOW:  Alpha power ∝ 1/precision
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_modulate_alpha_from_precision(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Generate theta-gamma PAC from hierarchy
 *
 * WHAT: Create theta-gamma coupling based on FEP hierarchy
 * WHY:  PAC reflects hierarchical belief integration
 * HOW:  Gamma bursts at theta peaks for each hierarchy level
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_generate_theta_gamma_pac(
    oscillations_fep_bridge_t* bridge
);

/* ============================================================================
 * Oscillations → FEP Direction
 * ============================================================================ */

/**
 * @brief Derive precision from gamma/alpha ratio
 *
 * WHAT: Compute FEP precision from oscillatory power
 * WHY:  Precision = γ/α in oscillatory coding
 * HOW:  Extract gamma and alpha power, compute ratio
 *
 * @param bridge Oscillations-FEP bridge
 * @param precision Output precision value
 * @return 0 on success
 */
int oscillations_fep_derive_precision_from_ratio(
    oscillations_fep_bridge_t* bridge,
    float* precision
);

/**
 * @brief Weight prediction errors by gamma power
 *
 * WHAT: Scale FEP prediction errors by gamma amplitude
 * WHY:  Gamma power indicates error salience
 * HOW:  Apply gamma-based weighting to FEP errors
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_weight_errors_by_gamma(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Bind beliefs via gamma coherence
 *
 * WHAT: Use gamma phase coherence for cross-regional binding
 * WHY:  Coherent gamma binds distributed beliefs
 * HOW:  Apply coherence-based coupling to FEP beliefs
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_bind_beliefs_via_coherence(
    oscillations_fep_bridge_t* bridge
);

/* ============================================================================
 * Analysis Functions
 * ============================================================================ */

/**
 * @brief Compute oscillatory band power
 *
 * WHAT: Extract power in each frequency band
 * WHY:  Monitor oscillatory state
 * HOW:  Analyze oscillation state phasors
 *
 * @param bridge Oscillations-FEP bridge
 * @param band_power Output band power
 * @return 0 on success
 */
int oscillations_fep_compute_band_power(
    oscillations_fep_bridge_t* bridge,
    oscillation_band_power_t* band_power
);

/**
 * @brief Detect theta-gamma PAC
 *
 * WHAT: Measure phase-amplitude coupling
 * WHY:  PAC indicates hierarchical integration
 * HOW:  Compute modulation index for theta phase / gamma amplitude
 *
 * @param bridge Oscillations-FEP bridge
 * @param pac_strength Output PAC strength
 * @param preferred_phase Output preferred theta phase
 * @return 0 on success
 */
int oscillations_fep_detect_pac(
    oscillations_fep_bridge_t* bridge,
    float* pac_strength,
    float* preferred_phase
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update Oscillations-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep FEP and oscillations synchronized
 * HOW:  Modulate oscillations from FEP, derive precision from oscillations
 *
 * @param bridge Oscillations-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int oscillations_fep_bridge_update(
    oscillations_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge Oscillations-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int oscillations_fep_bridge_get_state(
    const oscillations_fep_bridge_t* bridge,
    oscillations_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Oscillations-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int oscillations_fep_bridge_get_stats(
    const oscillations_fep_bridge_t* bridge,
    oscillations_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for FEP-oscillations coordination
 * WHY:  Distributed oscillatory signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_bridge_connect_bio_async(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge Oscillations-FEP bridge
 * @return 0 on success
 */
int oscillations_fep_bridge_disconnect_bio_async(
    oscillations_fep_bridge_t* bridge
);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Oscillations-FEP bridge
 * @return true if bio-async enabled
 */
bool oscillations_fep_bridge_is_bio_async_connected(
    const oscillations_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_OSCILLATIONS_FEP_BRIDGE_H */
