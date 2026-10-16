/**
 * @file nimcp_brain_immune_plasticity.h
 * @brief Brain Immune System - Plasticity Modulation Integration
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Integration layer connecting brain immune system with plasticity mechanisms
 * WHY:  Immune activation affects synaptic plasticity, attention, and learning
 * HOW:  Cytokines and inflammation levels modulate BCM thresholds, STDP timing windows,
 *       and attention mechanisms based on biological immune-neural interactions
 *
 * BIOLOGICAL MODEL:
 * ```
 * IMMUNE STATE              PLASTICITY EFFECT
 * ───────────────────────────────────────────────────────────────────
 * Inflammation (IL-1β)   → BCM threshold elevation (harder LTP)
 * IL-6 activation        → STDP timing window narrowing
 * TNF-α signaling        → Attention impairment (reduced gate opening)
 * IL-10 resolution       → Plasticity normalization
 * Chronic inflammation   → Persistent learning deficits
 * ```
 *
 * INTEGRATION ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    BRAIN IMMUNE - PLASTICITY BRIDGE                        ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE STATE (Cytokines + Inflammation)               │  ║
 * ║   │   IL-1β  │  IL-6  │  TNF-α  │  IL-10  │  Inflammation Level      │  ║
 * ║   └──────┬────────┬────────┬─────────┬───────────────┬─────────────────┘  ║
 * ║          │        │        │         │               │                    ║
 * ║          ▼        ▼        ▼         ▼               ▼                    ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                    MODULATION FACTORS                               │  ║
 * ║   │   BCM Threshold  │  STDP Windows  │  Attention Gate  │  LR Scale   │  ║
 * ║   └──────┬────────────────┬────────────────┬─────────────────┬─────────┘  ║
 * ║          │                │                │                 │            ║
 * ║          ▼                ▼                ▼                 ▼            ║
 * ║   ┌─────────────────────────────────────────────────────────────────────┐ ║
 * ║   │                      PLASTICITY MECHANISMS                           │ ║
 * ║   │        BCM           │        STDP         │      Attention          │ ║
 * ║   └─────────────────────────────────────────────────────────────────────┘ ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * BIOLOGICAL BASIS:
 * - IL-1β: Pro-inflammatory, inhibits LTP, elevates BCM threshold
 * - IL-6: Acute phase, narrows STDP timing windows
 * - TNF-α: Severe inflammation, impairs attention and working memory
 * - IL-10: Anti-inflammatory, restores normal plasticity
 * - Chronic inflammation: Associated with cognitive decline
 *
 * RESEARCH FOUNDATIONS:
 * - Stellwagen & Malenka (2006): TNF-α regulates synaptic scaling
 * - Goshen et al. (2008): IL-1 impairs hippocampal LTP and memory
 * - Yirmiya & Goshen (2011): Immune modulation of neural plasticity
 * - Prinz & Priller (2014): Neuroimmune crosstalk in brain function
 *
 * NIMCP STANDARDS:
 * - All functions < 50 lines
 * - Guard clauses (early returns)
 * - WHAT-WHY-HOW documentation
 * - Thread-safe where applicable
 * - nimcp_malloc/nimcp_free memory management
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_BRAIN_IMMUNE_PLASTICITY_H
#define NIMCP_BRAIN_IMMUNE_PLASTICITY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/bcm/nimcp_bcm.h"
#include "plasticity/stdp/nimcp_stdp.h"
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"
#include "plasticity/dendritic/nimcp_dendritic.h"
#include "plasticity/adaptive/nimcp_adaptive.h"
#include "plasticity/eligibility/nimcp_eligibility_trace.h"
#include "plasticity/predictive/nimcp_predictive_coding.h"
#include "plasticity/attention/nimcp_attention.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define IMMUNE_PLASTICITY_MAX_CYTOKINE_EFFECT    0.9f   /**< Max cytokine modulation */
#define IMMUNE_PLASTICITY_BASELINE_FACTOR        1.0f   /**< No immune activation */
#define IMMUNE_PLASTICITY_SEVERE_IMPAIRMENT      0.1f   /**< Severe inflammation */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Immune modulation factors for plasticity
 *
 * WHAT: Computed modulation factors based on immune state
 * WHY:  Central structure for applying immune effects to plasticity
 * HOW:  Factors are multiplied with plasticity parameters
 */
typedef struct {
    /* BCM modulation */
    float bcm_threshold_scale;      /**< BCM threshold multiplier (>1 = harder LTP) */
    float bcm_learning_rate_scale;  /**< BCM learning rate multiplier */

    /* STDP modulation */
    float stdp_tau_plus_scale;      /**< STDP LTP window scale (<1 = narrower) */
    float stdp_tau_minus_scale;     /**< STDP LTD window scale (<1 = narrower) */
    float stdp_learning_rate_scale; /**< STDP learning rate multiplier */

    /* STP modulation */
    float stp_u_scale;              /**< STP release probability scale */
    float stp_tau_d_scale;          /**< STP depression tau scale */
    float stp_tau_f_scale;          /**< STP facilitation tau scale */

    /* Homeostatic plasticity modulation */
    float homeostatic_scaling_rate; /**< Synaptic scaling rate multiplier */
    float homeostatic_target_shift; /**< Target firing rate shift (Hz) */
    float metaplasticity_theta_shift; /**< BCM theta shift during inflammation */

    /* Dendritic modulation */
    float nmda_conductance_scale;   /**< NMDA conductance multiplier */
    float dendritic_spike_threshold_shift; /**< Dendritic spike threshold shift (mV) */
    float ca_influx_scale;          /**< Calcium influx multiplier */

    /* Adaptive plasticity modulation */
    float adaptive_threshold_shift; /**< Adaptive threshold shift */
    float adaptive_sparsity_target; /**< Target sparsity shift */

    /* Eligibility trace modulation */
    float eligibility_decay_scale;  /**< Eligibility decay rate multiplier */
    float eligibility_learning_rate_scale; /**< Eligibility learning rate multiplier */

    /* Predictive coding modulation */
    float pc_prediction_precision_scale; /**< Prediction precision multiplier */
    float pc_error_weight_scale;    /**< Prediction error weight multiplier */
    float pc_learning_rate_scale;   /**< Predictive coding learning rate multiplier */

    /* Attention modulation */
    float attention_gate_scale;     /**< Attention gate multiplier (<1 = impaired) */
    float attention_temperature;    /**< Attention temperature increase (>0 = diffuse) */

    /* Overall plasticity */
    float global_plasticity_scale;  /**< Global plasticity factor */

    /* State tracking */
    brain_inflammation_level_t inflammation_level; /**< Current inflammation level */
    float il1_concentration;        /**< IL-1β level (0-1) */
    float il6_concentration;        /**< IL-6 level (0-1) */
    float tnf_alpha_concentration;  /**< TNF-α level (0-1) */
    float il10_concentration;       /**< IL-10 level (0-1) */
} immune_plasticity_modulation_t;

/**
 * @brief Configuration for immune-plasticity integration
 */
typedef struct {
    /* Cytokine sensitivity parameters */
    float il1_threshold_sensitivity;   /**< How much IL-1β affects BCM threshold */
    float il6_timing_sensitivity;      /**< How much IL-6 affects STDP windows */
    float tnf_attention_sensitivity;   /**< How much TNF-α affects attention */
    float il10_recovery_rate;          /**< How fast IL-10 restores plasticity */

    /* Inflammation thresholds */
    float local_inflammation_threshold;     /**< Threshold for local effects */
    float regional_inflammation_threshold;  /**< Threshold for regional effects */
    float systemic_inflammation_threshold;  /**< Threshold for systemic effects */

    /* Modulation bounds */
    float min_plasticity_factor;       /**< Minimum plasticity (severe inflammation) */
    float max_threshold_elevation;     /**< Maximum BCM threshold increase */
    float max_timing_narrowing;        /**< Maximum STDP window narrowing */
    float max_attention_impairment;    /**< Maximum attention reduction */
} immune_plasticity_config_t;

/**
 * @brief Statistics for immune-plasticity interactions
 */
typedef struct {
    uint64_t bcm_modulation_events;         /**< BCM modulations applied */
    uint64_t stdp_modulation_events;        /**< STDP modulations applied */
    uint64_t attention_modulation_events;   /**< Attention modulations applied */

    float avg_bcm_threshold_elevation;      /**< Average BCM threshold increase */
    float avg_stdp_window_reduction;        /**< Average STDP window narrowing */
    float avg_attention_impairment;         /**< Average attention reduction */

    uint32_t inflammation_events;           /**< Inflammation state changes */
    uint64_t cytokine_updates;              /**< Cytokine level updates */
} immune_plasticity_stats_t;

/* ============================================================================
 * Configuration API
 * ============================================================================ */

/**
 * @brief Get default immune-plasticity configuration
 *
 * WHAT: Provide biologically plausible default parameters
 * WHY:  Easy initialization with research-based values
 * HOW:  Return struct with parameters from literature
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int immune_plasticity_default_config(immune_plasticity_config_t* config);

/* ============================================================================
 * Modulation Computation API
 * ============================================================================ */

/**
 * @brief Compute immune modulation factors from immune state
 *
 * WHAT: Calculate all plasticity modulation factors based on immune system
 * WHY:  Central computation of immune effects on plasticity
 * HOW:  Read cytokine levels and inflammation, compute factor struct
 *
 * @param immune_system Brain immune system
 * @param config Immune-plasticity configuration
 * @param modulation Output modulation factors
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(num_cytokines + num_inflammation_sites)
 * PERFORMANCE: ~10μs typical
 */
int immune_plasticity_compute_modulation(
    const brain_immune_system_t* immune_system,
    const immune_plasticity_config_t* config,
    immune_plasticity_modulation_t* modulation
);

/**
 * @brief Compute cytokine concentration for specific type
 *
 * WHAT: Sum all cytokines of given type across immune system
 * WHY:  Need total concentration to compute effects
 * HOW:  Iterate cytokines, sum concentrations of matching type
 *
 * @param immune_system Brain immune system
 * @param cytokine_type Cytokine type to sum
 * @return Total concentration (0-1), clamped at 1.0
 *
 * COMPLEXITY: O(num_cytokines)
 */
float immune_plasticity_get_cytokine_concentration(
    const brain_immune_system_t* immune_system,
    brain_cytokine_type_t cytokine_type
);

/**
 * @brief Compute maximum inflammation level across all sites
 *
 * WHAT: Find highest inflammation level in system
 * WHY:  Most severe inflammation determines global effects
 * HOW:  Scan inflammation sites, return maximum
 *
 * @param immune_system Brain immune system
 * @return Maximum inflammation level
 *
 * COMPLEXITY: O(num_inflammation_sites)
 */
brain_inflammation_level_t immune_plasticity_get_max_inflammation(
    const brain_immune_system_t* immune_system
);

/* ============================================================================
 * BCM Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to BCM parameters
 *
 * WHAT: Modify BCM learning parameters based on immune state
 * WHY:  Inflammation elevates BCM threshold, impairs learning
 * HOW:  Scale threshold and learning rate by modulation factors
 *
 * BIOLOGICAL:
 * - IL-1β elevation → Higher BCM threshold (harder to induce LTP)
 * - Inflammation → Reduced learning rate
 * - IL-10 → Restore normal parameters
 *
 * @param params BCM parameters to modulate (modified in-place)
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 *
 * USAGE:
 * ```c
 * bcm_params_t params = bcm_params_cortical();
 * immune_plasticity_modulate_bcm(&params, &modulation);
 * // params now reflect immune state
 * ```
 */
int immune_plasticity_modulate_bcm(
    bcm_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to BCM threshold directly
 *
 * WHAT: Modify existing BCM synapse threshold based on immune state
 * WHY:  Real-time adjustment during ongoing learning
 * HOW:  Multiply threshold by immune scale factor
 *
 * @param synapse BCM synapse to modulate
 * @param modulation Immune modulation factors
 * @return New threshold value
 */
float immune_plasticity_modulate_bcm_threshold(
    bcm_synapse_t* synapse,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * STDP Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to STDP configuration
 *
 * WHAT: Modify STDP timing windows and learning rate based on immune state
 * WHY:  Inflammation narrows STDP timing windows, reduces learning
 * HOW:  Scale tau_plus, tau_minus, and learning rate by factors
 *
 * BIOLOGICAL:
 * - IL-6 → Narrower STDP timing windows (more precise timing required)
 * - TNF-α → Reduced STDP learning rate
 * - Chronic inflammation → Impaired spike-timing plasticity
 *
 * @param config STDP configuration to modulate (modified in-place)
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 *
 * USAGE:
 * ```c
 * stdp_config_t config = stdp_config_default();
 * immune_plasticity_modulate_stdp(&config, &modulation);
 * stdp_synapse_init_with_config(&synapse, &config);
 * ```
 */
int immune_plasticity_modulate_stdp(
    stdp_config_t* config,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to STDP timing windows directly
 *
 * WHAT: Modify existing STDP synapse timing constants
 * WHY:  Real-time adjustment during ongoing learning
 * HOW:  Scale tau_plus and tau_minus by immune factors
 *
 * @param synapse STDP synapse to modulate
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_stdp_timing(
    stdp_synapse_t* synapse,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Attention Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to attention configuration
 *
 * WHAT: Modify attention parameters based on immune state
 * WHY:  Inflammation impairs attention and cognitive control
 * HOW:  Reduce gate bias, increase temperature (diffuse attention)
 *
 * BIOLOGICAL:
 * - TNF-α → Impaired thalamic gating (reduced attention)
 * - Inflammation → Diffuse, unfocused attention
 * - IL-10 → Restore normal attention
 *
 * @param config Attention configuration to modulate (modified in-place)
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 *
 * USAGE:
 * ```c
 * multihead_attention_config_t config = {...};
 * immune_plasticity_modulate_attention_config(&config, &modulation);
 * multihead_attention_t mha = multihead_attention_create(&config);
 * ```
 */
int immune_plasticity_modulate_attention_config(
    multihead_attention_config_t* config,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to active attention system
 *
 * WHAT: Adjust running attention system based on immune state
 * WHY:  Real-time attention impairment during inflammation
 * HOW:  Update thalamic gate based on immune factors
 *
 * @param mha Multihead attention system
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 *
 * COMPLEXITY: O(1)
 * PERFORMANCE: ~1μs
 */
int immune_plasticity_modulate_attention_gate(
    multihead_attention_t mha,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Monitoring and Statistics API
 * ============================================================================ */

/**
 * @brief Get immune-plasticity statistics
 *
 * WHAT: Retrieve statistics about immune-plasticity interactions
 * WHY:  Monitor system behavior and immune effects
 * HOW:  Copy internal statistics to output struct
 *
 * @param stats Output statistics structure
 * @return 0 on success, -1 on error
 */
int immune_plasticity_get_stats(immune_plasticity_stats_t* stats);

/**
 * @brief Reset immune-plasticity statistics
 *
 * WHAT: Clear all accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero statistics structure
 */
void immune_plasticity_reset_stats(void);

/**
 * @brief Check if immune state significantly impairs plasticity
 *
 * WHAT: Determine if plasticity is meaningfully affected
 * WHY:  Quick check for severe immune effects
 * HOW:  Compare global plasticity scale to threshold
 *
 * @param modulation Immune modulation factors
 * @param threshold Impairment threshold (default: 0.7)
 * @return true if plasticity impaired below threshold
 */
bool immune_plasticity_is_impaired(
    const immune_plasticity_modulation_t* modulation,
    float threshold
);

/* ============================================================================
 * STP Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to STP parameters
 *
 * WHAT: Modify STP parameters based on immune state
 * WHY:  Inflammation affects neurotransmitter release dynamics
 * HOW:  Scale U (release probability), tau_D, and tau_F by immune factors
 *
 * BIOLOGICAL:
 * - IL-1β → Reduced neurotransmitter release (lower U)
 * - Inflammation → Slower recovery (increased tau_D)
 * - TNF-α → Impaired facilitation (increased tau_F)
 *
 * @param params STP parameters to modulate (modified in-place)
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_stp(
    stp_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to STP state
 *
 * WHAT: Modify active STP synapse based on immune state
 * WHY:  Real-time adjustment of release dynamics
 * HOW:  Update U, tau_D, tau_F in synapse state
 *
 * @param state STP state to modulate
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_stp_state(
    stp_state_t* state,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Homeostatic Plasticity Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to homeostatic plasticity config
 *
 * WHAT: Modify homeostatic mechanisms based on immune state
 * WHY:  Inflammation disrupts homeostatic stability
 * HOW:  Adjust target rates, scaling speed, and metaplasticity thresholds
 *
 * BIOLOGICAL:
 * - Chronic inflammation → Shifted firing rate setpoints
 * - IL-1β → Slower homeostatic compensation
 * - IL-10 → Faster restoration of homeostasis
 *
 * @param config Homeostatic config to modulate (modified in-place)
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_homeostatic_config(
    homeostatic_config_t* config,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to synaptic scaling parameters
 *
 * WHAT: Adjust synaptic scaling dynamics
 * WHY:  Inflammation affects global synaptic scaling
 * HOW:  Modify target rate and scaling time constant
 *
 * @param params Synaptic scaling parameters
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_synaptic_scaling(
    synaptic_scaling_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to metaplasticity parameters
 *
 * WHAT: Adjust BCM sliding threshold dynamics
 * WHY:  Inflammation changes metaplastic state
 * HOW:  Shift theta and adjust time constants
 *
 * @param params Metaplasticity parameters
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_metaplasticity(
    metaplasticity_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Dendritic Nonlinearity Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to NMDA parameters
 *
 * WHAT: Modify NMDA receptor dynamics based on immune state
 * WHY:  Inflammation affects NMDA receptor function and calcium influx
 * HOW:  Scale conductance, adjust kinetics, modify calcium permeability
 *
 * BIOLOGICAL:
 * - IL-1β → Reduced NMDA conductance
 * - TNF-α → Impaired calcium signaling
 * - Chronic inflammation → NMDA receptor internalization
 *
 * @param params NMDA parameters to modulate
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_nmda(
    nmda_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to dendritic compartment parameters
 *
 * WHAT: Adjust dendritic spike thresholds and integration
 * WHY:  Inflammation affects dendritic excitability
 * HOW:  Shift spike thresholds, modify supralinearity
 *
 * @param params Compartment parameters
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_dendritic_compartment(
    compartment_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Adaptive Plasticity Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to adaptive spiking parameters
 *
 * WHAT: Modify adaptive threshold and sparsity targets
 * WHY:  Inflammation affects neural excitability and sparsity
 * HOW:  Shift adaptive thresholds and sparsity targets
 *
 * BIOLOGICAL:
 * - Inflammation → Higher firing thresholds
 * - TNF-α → Increased sparsity (fewer active neurons)
 * - IL-10 → Restoration of normal excitability
 *
 * @param params Adaptive spike parameters
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_adaptive_params(
    adaptive_spike_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Eligibility Trace Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to eligibility trace config
 *
 * WHAT: Modify eligibility trace decay and learning rates
 * WHY:  Inflammation affects temporal credit assignment
 * HOW:  Adjust decay lambda and learning rates
 *
 * BIOLOGICAL:
 * - Inflammation → Faster trace decay (shorter credit window)
 * - TNF-α → Reduced eligibility-based learning
 * - IL-10 → Restored temporal credit assignment
 *
 * @param config Eligibility config to modulate
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_eligibility_config(
    eligibility_config_t* config,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * Predictive Coding Integration API
 * ============================================================================ */

/**
 * @brief Apply immune modulation to predictive coding layer parameters
 *
 * WHAT: Modify prediction precision and error weighting
 * WHY:  Inflammation affects prediction and error processing
 * HOW:  Adjust precisions, learning rates, and error weights
 *
 * BIOLOGICAL:
 * - Inflammation → Reduced prediction precision (increased uncertainty)
 * - IL-6 → Impaired error correction
 * - TNF-α → Bias toward prediction over sensory evidence
 *
 * @param params PC layer parameters
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_predictive_coding_layer(
    pc_layer_params_t* params,
    const immune_plasticity_modulation_t* modulation
);

/**
 * @brief Apply immune modulation to predictive coding hierarchy
 *
 * WHAT: Modify entire hierarchy configuration
 * WHY:  Global effects on predictive processing
 * HOW:  Adjust precision learning and prediction strength
 *
 * @param config PC hierarchy config
 * @param modulation Immune modulation factors
 * @return 0 on success, -1 on error
 */
int immune_plasticity_modulate_predictive_coding_hierarchy(
    pc_hierarchy_config_t* config,
    const immune_plasticity_modulation_t* modulation
);

/* ============================================================================
 * String Conversion Utilities
 * ============================================================================ */

/**
 * @brief Get string description of modulation state
 *
 * WHAT: Human-readable summary of immune effects on plasticity
 * WHY:  Debugging and logging
 * HOW:  Format modulation factors as string
 *
 * @param modulation Modulation factors
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written
 */
int immune_plasticity_modulation_to_string(
    const immune_plasticity_modulation_t* modulation,
    char* buffer,
    size_t buffer_size
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BRAIN_IMMUNE_PLASTICITY_H */
