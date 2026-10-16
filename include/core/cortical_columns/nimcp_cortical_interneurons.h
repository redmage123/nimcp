/**
 * @file nimcp_cortical_interneurons.h
 * @brief Cortical Interneuron System - Inhibitory Circuit Types and E/I Balance
 * @version 1.0.0
 * @date 2026-03-05
 *
 * WHAT: Models the five major classes of cortical GABAergic interneurons that shape
 *       neural computation through inhibition, disinhibition, and oscillatory control.
 * WHY:  Cortical computation depends critically on interneuron diversity. Different
 *       interneuron types target distinct subcellular compartments and implement specific
 *       circuit motifs (feedforward inhibition, feedback inhibition, disinhibition).
 *       Without accurate interneuron modeling, E/I balance, gamma oscillations, and
 *       attentional gating cannot be faithfully represented.
 * HOW:  Each interneuron type has biologically-calibrated firing rates, inhibition
 *       strengths, and target specificity. The system maintains global E/I balance
 *       (~4:1 ratio), tracks gamma power from PV basket cells, and provides
 *       VIP-mediated disinhibition for attention gating.
 *
 * BIOLOGICAL BASIS:
 * =================================================================================
 *
 * PV BASKET CELLS (Fast-Spiking):
 * --------------------------------
 * - Target: Perisomatic region (soma + proximal dendrites)
 * - Function: Precise timing control, gamma oscillation generation (30-80 Hz)
 * - Firing rate: Up to 300 Hz, fast-spiking phenotype
 * - Reference: Hu et al. (2014) "Interneurons: Fast-spiking, parvalbumin(+)
 *              GABAergic interneurons: From cellular design to microcircuit function"
 *
 * PV CHANDELIER CELLS (Axo-Axonic):
 * ----------------------------------
 * - Target: Axon initial segment (AIS) of pyramidal cells
 * - Function: Gate action potential initiation, veto mechanism
 * - Effect: Can be depolarizing (excitatory) or hyperpolarizing depending on
 *           GABA reversal potential at AIS
 * - Reference: Woodruff et al. (2010) "Depolarizing effect of neocortical
 *              chandelier neurons"
 *
 * SST MARTINOTTI CELLS (Dendrite-Targeting):
 * -------------------------------------------
 * - Target: Distal apical dendrites in Layer 1
 * - Function: Feedback inhibition from L5 to L1, prediction error computation
 * - Soma in L5/6, axon extends to L1 (long-range projections)
 * - Reference: Murayama et al. (2009) "Dendritic encoding of sensory stimuli
 *              controlled by deep cortical interneurons"
 *
 * VIP CELLS (Disinhibitory):
 * ---------------------------
 * - Target: SST and PV interneurons (inhibit the inhibitors)
 * - Function: Disinhibition, attention gating, behavioral state control
 * - Activated by top-down attention and cholinergic modulation
 * - Reference: Pi et al. (2013) "Cortical interneurons that specialize in
 *              disinhibitory control"
 *
 * NGF LAYER 1 CELLS (Volume Transmission):
 * -----------------------------------------
 * - Target: Broad dendritic field via GABA volume transmission
 * - Function: Slow, tonic inhibition affecting entire dendritic arbors
 * - Provides persistent inhibitory tone in superficial layers
 * - Reference: Olah et al. (2009) "Regulation of cortical microcircuits by
 *              unitary GABA-mediated volume transmission"
 *
 * E/I BALANCE:
 * ------------
 * - Target ratio ~4:1 (excitatory:inhibitory) in healthy cortex
 * - Disruption linked to epilepsy (excess E), autism, schizophrenia
 * - Reference: Isaacson & Scanziani (2011) "How inhibition shapes cortical activity"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_INTERNEURONS_H
#define NIMCP_CORTICAL_INTERNEURONS_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/thread/nimcp_thread.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define CINT_MAGIC 0xC10E0A0D

/** Default interneuron counts per type (biologically calibrated ratios) */
#define CINT_DEFAULT_PV_BASKET      40
#define CINT_DEFAULT_PV_CHANDELIER  10
#define CINT_DEFAULT_SST            25
#define CINT_DEFAULT_VIP            15
#define CINT_DEFAULT_NGF            10

/** E/I balance target (~4:1 excitatory to inhibitory) */
#define CINT_TARGET_EI_RATIO        4.0f

/** Gamma oscillation band (Hz) */
#define CINT_GAMMA_LOW_HZ           30.0f
#define CINT_GAMMA_HIGH_HZ          80.0f

/* ============================================================================
 * Types and Enumerations
 * ============================================================================ */

/**
 * @brief Cortical interneuron type classification
 *
 * WHAT: Five major classes of GABAergic cortical interneurons
 * WHY:  Each type implements distinct circuit motifs and targets different
 *       subcellular compartments of pyramidal cells
 */
typedef enum {
    CINT_PV_BASKET,      /**< Fast-spiking, perisomatic inhibition, gamma oscillations */
    CINT_PV_CHANDELIER,  /**< Axo-axonic, gates AP initiation at AIS */
    CINT_SST_MARTINOTTI, /**< Dendrite-targeting, L5->L1 feedback inhibition */
    CINT_VIP,            /**< Disinhibition (inhibit SST/PV), attention gating */
    CINT_NGF_L1,         /**< Volume transmission GABA, slow tonic inhibition */
    CINT_TYPE_COUNT
} cortical_interneuron_type_t;

/**
 * @brief Per-interneuron dynamic state
 *
 * WHAT: Runtime state for a single interneuron unit
 * WHY:  Track firing dynamics, inhibition strength, and E/I contribution
 */
typedef struct {
    cortical_interneuron_type_t type;
    float firing_rate;            /**< Current firing rate (Hz) */
    float inhibition_strength;    /**< Output inhibition strength [0.0-1.0] */
    float membrane_potential;     /**< Membrane potential (mV) */
    float threshold;              /**< Spike threshold (mV) */
    float refractory_ms;          /**< Refractory period remaining (ms) */
    uint32_t target_count;        /**< Number of postsynaptic targets */
    float ei_ratio;               /**< Contribution to E/I ratio */
} interneuron_state_t;

/**
 * @brief Configuration for the cortical interneuron system
 *
 * WHAT: Specifies interneuron counts per type and E/I balance target
 * WHY:  Allow tuning interneuron populations for different cortical areas
 */
typedef struct {
    uint32_t num_pv_basket;       /**< Number of PV basket cells */
    uint32_t num_pv_chandelier;   /**< Number of PV chandelier cells */
    uint32_t num_sst;             /**< Number of SST Martinotti cells */
    uint32_t num_vip;             /**< Number of VIP cells */
    uint32_t num_ngf;             /**< Number of NGF Layer 1 cells */
    float target_ei_ratio;        /**< Target excitation/inhibition ratio (default 4.0) */
} cint_config_t;

/**
 * @brief Statistics for the cortical interneuron system
 *
 * WHAT: Runtime metrics for monitoring interneuron system health
 * WHY:  Track E/I balance, gamma power, and disinhibition levels
 */
typedef struct {
    uint64_t total_updates;       /**< Total update cycles */
    float avg_pv_firing_rate;     /**< Average PV basket firing rate (Hz) */
    float avg_sst_firing_rate;    /**< Average SST firing rate (Hz) */
    float avg_vip_firing_rate;    /**< Average VIP firing rate (Hz) */
    float peak_gamma_power;       /**< Peak gamma power observed */
    float min_ei_balance;         /**< Minimum E/I balance observed */
    float max_ei_balance;         /**< Maximum E/I balance observed */
} cint_stats_t;

/**
 * @brief Cortical interneuron system
 *
 * WHAT: Complete inhibitory interneuron circuit system
 * WHY:  Manage all interneuron types, E/I balance, gamma oscillations,
 *       and attention gating via disinhibition
 */
typedef struct cortical_interneuron_system {
    uint32_t magic;               /**< Magic number for validation (0xC1NEUR0N) */
    cint_config_t config;         /**< System configuration */
    uint32_t num_interneurons;    /**< Total interneuron count (all types) */
    interneuron_state_t* interneurons; /**< Array of interneuron states */

    /* Derived system-level metrics */
    float gamma_power;            /**< 30-80 Hz oscillation power from PV basket cells */
    float ei_balance;             /**< Overall E/I ratio (target ~4:1) */
    float disinhibition_level;    /**< VIP-mediated disinhibition [0.0-1.0] */
    float prediction_error;       /**< SST Martinotti contribution to prediction error */

    /* Timing */
    uint64_t last_update_us;      /**< Timestamp of last update (microseconds) */

    /* Statistics */
    cint_stats_t stats;           /**< Accumulated statistics */

    /* Thread safety */
    nimcp_mutex_t* lock;          /**< Mutex for thread-safe access */
} cortical_interneuron_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default cortical interneuron configuration
 *
 * WHAT: Provide biologically-calibrated default parameters
 * WHY:  Easy initialization with realistic interneuron populations
 * HOW:  Sets PV:SST:VIP:NGF ratios based on cortical measurements
 *
 * @param config Output configuration (must not be NULL)
 * @return 0 on success, -1 on error
 */
int cint_default_config(cint_config_t* config);

/**
 * @brief Create cortical interneuron system
 *
 * WHAT: Allocate and initialize complete interneuron system
 * WHY:  Enable inhibitory circuit modeling in cortical columns
 * HOW:  Allocate interneuron array, set type-specific properties,
 *       initialize E/I balance tracking
 *
 * @param config Configuration (NULL for defaults)
 * @return New system or NULL on failure
 */
cortical_interneuron_system_t* cint_create(const cint_config_t* config);

/**
 * @brief Destroy cortical interneuron system
 *
 * WHAT: Free all resources associated with the interneuron system
 * WHY:  Proper cleanup and memory management
 * HOW:  Free interneuron array, destroy mutex, free system
 *
 * @param system System to destroy (NULL-safe)
 */
void cint_destroy(cortical_interneuron_system_t* system);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update interneuron system state
 *
 * WHAT: Advance all interneuron dynamics by dt_s seconds
 * WHY:  Simulate interneuron firing, refractory periods, and derived metrics
 * HOW:  Update membrane potentials, check thresholds, compute gamma power,
 *       recalculate E/I balance, update disinhibition level
 *
 * @param system Interneuron system
 * @param dt_s Time step in seconds
 * @return 0 on success, -1 on error
 */
int cint_update(cortical_interneuron_system_t* system, float dt_s);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current gamma oscillation power
 *
 * WHAT: Return PV basket cell-driven gamma (30-80 Hz) oscillation power
 * WHY:  Gamma power indicates cortical computation engagement and attention
 * HOW:  Derived from PV basket cell population firing synchrony
 *
 * @param system Interneuron system
 * @return Gamma power [0.0-1.0], or 0.0 on error
 */
float cint_get_gamma_power(const cortical_interneuron_system_t* system);

/**
 * @brief Get current E/I balance ratio
 *
 * WHAT: Return overall excitation/inhibition ratio
 * WHY:  E/I balance is critical for cortical stability (target ~4:1)
 * HOW:  Computed from aggregate inhibitory output vs. excitatory drive
 *
 * @param system Interneuron system
 * @return E/I ratio (target ~4.0), or -1.0 on error
 */
float cint_get_ei_balance(const cortical_interneuron_system_t* system);

/**
 * @brief Get VIP-mediated disinhibition level
 *
 * WHAT: Return current disinhibition level from VIP interneurons
 * WHY:  Disinhibition gates attention and behavioral state transitions
 * HOW:  VIP cells inhibit SST/PV, releasing pyramidal cells from inhibition
 *
 * @param system Interneuron system
 * @return Disinhibition level [0.0-1.0], or 0.0 on error
 */
float cint_get_disinhibition(const cortical_interneuron_system_t* system);

/**
 * @brief Get SST Martinotti prediction error contribution
 *
 * WHAT: Return prediction error signal from SST cells
 * WHY:  SST cells provide feedback inhibition that encodes prediction error
 *       (mismatch between top-down predictions and bottom-up input)
 * HOW:  Derived from SST cell firing relative to expected baseline
 *
 * @param system Interneuron system
 * @return Prediction error [0.0-1.0], or 0.0 on error
 */
float cint_get_prediction_error(const cortical_interneuron_system_t* system);

/* ============================================================================
 * Modulation API
 * ============================================================================ */

/**
 * @brief Modulate interneuron system by attention level
 *
 * WHAT: Apply top-down attention signal to interneuron circuits
 * WHY:  Attention activates VIP cells, causing disinhibition of pyramidal cells
 *       and enhancement of PV-driven gamma oscillations
 * HOW:  Increase VIP firing rate proportional to attention, boost PV gamma
 *
 * @param system Interneuron system
 * @param attention_level Attention level [0.0-1.0]
 * @return 0 on success, -1 on error
 */
int cint_modulate_attention(cortical_interneuron_system_t* system,
                            float attention_level);

/**
 * @brief Get system statistics
 *
 * WHAT: Retrieve accumulated interneuron system statistics
 * WHY:  Monitor system health and performance
 *
 * @param system Interneuron system
 * @param stats Output statistics
 * @return 0 on success, -1 on error
 */
int cint_get_stats(const cortical_interneuron_system_t* system,
                   cint_stats_t* stats);

/**
 * @brief Reset system statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 *
 * @param system Interneuron system
 */
void cint_reset_stats(cortical_interneuron_system_t* system);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_CORTICAL_INTERNEURONS_H */
