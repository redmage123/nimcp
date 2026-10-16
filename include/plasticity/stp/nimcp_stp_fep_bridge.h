/**
 * @file nimcp_stp_fep_bridge.h
 * @brief Free Energy Principle - Short-Term Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Bidirectional integration between Free Energy Principle and short-term plasticity (STP)
 * WHY:  FEP precision modulates STP dynamics; STP state provides precision estimates for FEP.
 *       Essential for adaptive gain control in predictive processing under active inference.
 * HOW:  FEP precision modulates facilitation/depression strength; prediction error scales STP dynamics;
 *       STP facilitation/depression state informs FEP precision estimates about signal reliability.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * FEP → STP PATHWAYS:
 * -------------------
 * 1. Precision Controls STP Strength:
 *    - High precision → stronger facilitation (amplify expected signals)
 *    - Low precision → stronger depression (suppress unreliable signals)
 *    - Precision as synaptic gain control mechanism
 *    - Reference: Friston & Buzsáki (2016) "The functional anatomy of time"
 *
 * 2. Prediction Error Modulates Release Probability:
 *    - High PE → increased U (baseline release probability)
 *    - Low PE → decreased U (stable synaptic transmission)
 *    - PE-driven synaptic adaptation on sub-second timescales
 *    - Reference: Tsodyks & Markram (1997) "The neural code between neocortical pyramidal neurons"
 *
 * 3. Free Energy Regulates Recovery Dynamics:
 *    - High F → faster recovery (τ_D decrease)
 *    - Low F → slower recovery (maintain depletion state)
 *    - Energy minimization through temporal filtering
 *
 * STP → FEP PATHWAYS:
 * -------------------
 * 1. Facilitation State Indicates Precision:
 *    - High u (facilitation) → high precision (reliable signal source)
 *    - Low u → low precision (uncertain signal)
 *    - STP state as dynamic precision estimate
 *
 * 2. Depression State Indicates Complexity Cost:
 *    - Low x (depletion) → high complexity (resource-intensive coding)
 *    - High x → low complexity (efficient coding)
 *    - Resource availability constrains free energy
 *
 * 3. Effective Synaptic Strength (u*x) as Belief Update:
 *    - u*x modulates prediction error transmission
 *    - Short-term synaptic plasticity implements temporal precision weighting
 *    - Adaptive filtering of prediction errors
 *
 * INTEGRATION MECHANISMS:
 * -----------------------
 * - Precision-scaled release: U_effective = U_base × f(precision)
 * - PE-scaled recovery: τ_D_effective = τ_D_base / (1 + |PE|)
 * - STP-to-precision mapping: precision_estimate = g(u, x)
 * - Temporal filtering: FEP precision weighted by STP modulation (u*x)
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

#ifndef NIMCP_STP_FEP_BRIDGE_H
#define NIMCP_STP_FEP_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/stp/nimcp_stp.h"
#include "cognitive/free_energy/nimcp_free_energy.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Prediction error scaling factors */
#define STP_FEP_PE_MIN_THRESHOLD        0.1f    /**< Min PE for modulation */
#define STP_FEP_PE_MAX_THRESHOLD        5.0f    /**< Max PE (saturation) */
#define STP_FEP_PE_SCALING_FACTOR       0.5f    /**< PE → U modulation */

/* Precision modulation factors */
#define STP_FEP_PRECISION_MIN           0.5f    /**< Min precision scaling */
#define STP_FEP_PRECISION_MAX           2.0f    /**< Max precision boost */
#define STP_FEP_PRECISION_SENSITIVITY   1.0f    /**< Precision → facilitation scaling */

/* Release probability control */
#define STP_FEP_U_MIN                   0.05f   /**< Min release probability */
#define STP_FEP_U_MAX                   0.95f   /**< Max release probability */
#define STP_FEP_U_BASELINE              0.5f    /**< Baseline release probability */

/* Recovery dynamics */
#define STP_FEP_TAU_D_MIN_FACTOR        0.5f    /**< Min recovery time scaling */
#define STP_FEP_TAU_D_MAX_FACTOR        2.0f    /**< Max recovery time scaling */
#define STP_FEP_TAU_F_MIN_FACTOR        0.5f    /**< Min facilitation time scaling */
#define STP_FEP_TAU_F_MAX_FACTOR        2.0f    /**< Max facilitation time scaling */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct stp_fep_bridge stp_fep_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for STP-FEP bridge
 */
typedef struct {
    /* Thresholds */
    float pe_min_threshold;              /**< Min PE for modulation */
    float pe_max_threshold;              /**< Max PE (saturation) */
    float precision_sensitivity;         /**< Precision → facilitation scaling */
    float u_min;                         /**< Min release probability */
    float u_max;                         /**< Max release probability */

    /* Feature enables */
    bool enable_pe_modulation;           /**< PE modulates release probability */
    bool enable_precision_facilitation;  /**< Precision modulates facilitation */
    bool enable_free_energy_recovery;    /**< Free energy adjusts recovery dynamics */
    bool enable_stp_precision_feedback;  /**< STP state updates FEP precision */

    /* Sensitivity factors */
    float pe_sensitivity;                /**< PE effect scaling */
    float precision_gain;                /**< Precision effect scaling */
    float free_energy_gain;              /**< Free energy effect on recovery */
    float stp_precision_gain;            /**< STP → precision scaling */
} stp_fep_config_t;

/**
 * @brief FEP effects on STP
 */
typedef struct {
    /* Prediction error effects */
    float pe_magnitude;                  /**< Current PE magnitude */
    float pe_u_scaling;                  /**< PE → release probability scaling */

    /* Precision effects */
    float precision_value;               /**< Current precision */
    float precision_facilitation_scaling;/**< Precision → facilitation scaling */

    /* Free energy effects */
    float free_energy_value;             /**< Current free energy */
    float free_energy_recovery_scaling;  /**< Free energy → recovery time scaling */

    /* Total effects */
    float effective_u;                   /**< Modulated release probability */
    float effective_tau_d;               /**< Modulated depression time constant */
    float effective_tau_f;               /**< Modulated facilitation time constant */
} stp_fep_effects_t;

/**
 * @brief STP effects on FEP
 */
typedef struct {
    /* Facilitation feedback */
    float current_u;                     /**< Current facilitation level */
    float u_precision_estimate;          /**< Facilitation-derived precision */

    /* Depression feedback */
    float current_x;                     /**< Current resource level */
    float x_complexity_estimate;         /**< Depression-derived complexity */

    /* Effective transmission */
    float effective_transmission;        /**< u*x effective synaptic strength */
    float transmission_precision_weight; /**< Precision weighting for PE transmission */
} stp_fep_feedback_t;

/**
 * @brief Current state of STP-FEP interaction
 */
typedef struct {
    /* Current FEP state */
    float current_pe;                    /**< Current prediction error */
    float current_precision;             /**< Current precision estimate */
    float current_free_energy;           /**< Current free energy */

    /* Current STP state */
    float current_u;                     /**< Current facilitation (u) */
    float current_x;                     /**< Current resources (x) */
    float current_modulation;            /**< Current u*x modulation */

    /* Applied modifiers */
    float u_modulation;                  /**< Current U modifier */
    float tau_d_modulation;              /**< Current τ_D modifier */
    float tau_f_modulation;              /**< Current τ_F modifier */

    /* Statistics */
    uint32_t modulation_events;          /**< STP modulation events */
    uint64_t last_update_time;           /**< Last update timestamp */
} stp_fep_state_t;

/**
 * @brief Statistics for STP-FEP bridge
 */
typedef struct {
    /* Modulation events */
    uint64_t total_updates;              /**< Total bridge updates */
    uint64_t pe_modulated_events;        /**< PE-modulated events */
    uint64_t precision_modulated_events; /**< Precision-modulated events */

    /* Effect magnitudes */
    float avg_pe_scaling;                /**< Average PE scaling factor */
    float avg_precision_scaling;         /**< Average precision scaling */
    float avg_u_modulation;              /**< Average U modulation */

    /* STP state */
    float avg_facilitation;              /**< Average facilitation level */
    float avg_depression;                /**< Average depression level */
    float avg_effective_transmission;    /**< Average u*x */

    /* Performance */
    float avg_free_energy;               /**< Average free energy */
    float avg_prediction_error;          /**< Average PE */
} stp_fep_stats_t;

/**
 * @brief STP-FEP bridge state
 */
struct stp_fep_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    stp_fep_config_t config;

    /* Connected systems */
    fep_system_t* fep_system;            /**< FEP system */
    stp_state_t* stp_state;              /**< STP state */

    /* Current effects */
    stp_fep_effects_t fep_effects;
    stp_fep_feedback_t stp_effects;
    stp_fep_state_t state;

    /* Statistics */
    stp_fep_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default STP-FEP configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard thresholds and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int stp_fep_bridge_default_config(stp_fep_config_t* config);

/**
 * @brief Create STP-FEP bridge
 *
 * WHAT: Initialize STP-FEP integration bridge
 * WHY:  Enable bidirectional STP-FEP interaction
 * HOW:  Allocate bridge, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
stp_fep_bridge_t* stp_fep_bridge_create(const stp_fep_config_t* config);

/**
 * @brief Destroy STP-FEP bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void stp_fep_bridge_destroy(stp_fep_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect FEP system
 *
 * WHAT: Link bridge to FEP system
 * WHY:  Enable FEP state monitoring
 * HOW:  Store FEP system pointer
 *
 * @param bridge STP-FEP bridge
 * @param fep FEP system
 * @return 0 on success
 */
int stp_fep_bridge_connect_fep(
    stp_fep_bridge_t* bridge,
    fep_system_t* fep
);

/**
 * @brief Connect STP state
 *
 * WHAT: Link bridge to STP state
 * WHY:  Enable STP parameter modulation
 * HOW:  Store STP state pointer
 *
 * @param bridge STP-FEP bridge
 * @param stp_state STP state
 * @return 0 on success
 */
int stp_fep_bridge_connect_stp(
    stp_fep_bridge_t* bridge,
    stp_state_t* stp_state
);

/**
 * @brief Disconnect all systems
 *
 * WHAT: Unlink FEP and STP systems
 * WHY:  Safe shutdown
 * HOW:  Clear system pointers
 *
 * @param bridge STP-FEP bridge
 * @return 0 on success
 */
int stp_fep_bridge_disconnect(stp_fep_bridge_t* bridge);

/* ============================================================================
 * FEP → STP Direction
 * ============================================================================ */

/**
 * @brief Apply prediction error modulation to release probability
 *
 * WHAT: Modulate baseline release probability U by PE magnitude
 * WHY:  Unexpected events increase synaptic efficacy
 * HOW:  U_effective = U_base × (1 + pe_gain × |PE|)
 *
 * @param bridge STP-FEP bridge
 * @param pe Prediction error magnitude
 * @return Release probability scaling factor
 */
float stp_fep_apply_pe_modulation(
    stp_fep_bridge_t* bridge,
    float pe
);

/**
 * @brief Apply precision modulation to facilitation
 *
 * WHAT: Modulate facilitation dynamics by precision (confidence)
 * WHY:  High precision predictions benefit from stronger facilitation
 * HOW:  τ_F_effective = τ_F_base / f(precision)
 *
 * @param bridge STP-FEP bridge
 * @param precision Current precision estimate
 * @return Facilitation time constant scaling factor
 */
float stp_fep_apply_precision_facilitation(
    stp_fep_bridge_t* bridge,
    float precision
);

/**
 * @brief Apply free energy regularization to recovery dynamics
 *
 * WHAT: Adjust recovery time constants based on free energy
 * WHY:  Energy minimization through temporal filtering
 * HOW:  τ_D_effective = τ_D_base × (1 + F × gain)
 *
 * @param bridge STP-FEP bridge
 * @param free_energy Current free energy
 * @return Recovery time constant scaling factor
 */
float stp_fep_apply_free_energy_recovery(
    stp_fep_bridge_t* bridge,
    float free_energy
);

/**
 * @brief Get effective release probability
 *
 * WHAT: Compute FEP-modulated release probability
 * WHY:  Combine all FEP effects on U
 * HOW:  Apply precision and PE modulations to base U
 *
 * @param bridge STP-FEP bridge
 * @param base_u Base release probability
 * @return Effective release probability
 */
float stp_fep_get_effective_u(
    const stp_fep_bridge_t* bridge,
    float base_u
);

/* ============================================================================
 * STP → FEP Direction
 * ============================================================================ */

/**
 * @brief Report facilitation to FEP system
 *
 * WHAT: Update FEP precision estimates based on facilitation state
 * WHY:  Facilitation indicates signal reliability
 * HOW:  Convert u state to precision estimate
 *
 * @param bridge STP-FEP bridge
 * @param u Current facilitation level
 * @return 0 on success
 */
int stp_fep_report_facilitation(
    stp_fep_bridge_t* bridge,
    float u
);

/**
 * @brief Report depression to FEP system
 *
 * WHAT: Update FEP complexity estimates based on resource depletion
 * WHY:  Depression indicates coding efficiency
 * HOW:  Convert x state to complexity estimate
 *
 * @param bridge STP-FEP bridge
 * @param x Current resource level
 * @return 0 on success
 */
int stp_fep_report_depression(
    stp_fep_bridge_t* bridge,
    float x
);

/**
 * @brief Compute precision estimate from STP state
 *
 * WHAT: Derive precision from facilitation and depression
 * WHY:  STP state reflects signal quality
 * HOW:  precision ∝ u (high facilitation = high precision)
 *
 * @param bridge STP-FEP bridge
 * @return Precision estimate
 */
float stp_fep_compute_stp_precision(
    const stp_fep_bridge_t* bridge
);

/**
 * @brief Get effective transmission weight
 *
 * WHAT: Compute u*x effective synaptic strength
 * WHY:  This is the actual prediction error weight
 * HOW:  Multiply facilitation by resources
 *
 * @param bridge STP-FEP bridge
 * @return Effective transmission factor
 */
float stp_fep_get_effective_transmission(
    const stp_fep_bridge_t* bridge
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update STP-FEP bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Keep STP and FEP systems synchronized
 * HOW:  Update effects, apply modulation, track statistics
 *
 * @param bridge STP-FEP bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int stp_fep_bridge_update(
    stp_fep_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge STP-FEP bridge
 * @param state Output state
 * @return 0 on success
 */
int stp_fep_bridge_get_state(
    const stp_fep_bridge_t* bridge,
    stp_fep_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge STP-FEP bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int stp_fep_bridge_get_stats(
    const stp_fep_bridge_t* bridge,
    stp_fep_stats_t* stats
);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for STP-FEP coordination
 * WHY:  Distributed synaptic signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge STP-FEP bridge
 * @return 0 on success
 */
int stp_fep_bridge_connect_bio_async(stp_fep_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge STP-FEP bridge
 * @return 0 on success
 */
int stp_fep_bridge_disconnect_bio_async(stp_fep_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge STP-FEP bridge
 * @return true if bio-async enabled
 */
bool stp_fep_bridge_is_bio_async_connected(
    const stp_fep_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STP_FEP_BRIDGE_H */
