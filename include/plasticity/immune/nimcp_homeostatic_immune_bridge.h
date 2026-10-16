/**
 * @file nimcp_homeostatic_immune_bridge.h
 * @brief Homeostatic Plasticity-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and homeostatic plasticity
 * WHY:  Biological evidence shows immune-homeostasis coupling (cytokines disrupt setpoints,
 *       TNF-α regulates synaptic scaling, chronic inflammation causes abnormal adaptation).
 * HOW:  Cytokines modulate homeostatic setpoints and scaling factors, inflammation affects
 *       target firing rates, anti-inflammatory signals restore normal homeostasis.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → HOMEOSTASIS PATHWAYS:
 * ------------------------------
 * 1. TNF-α and Synaptic Scaling:
 *    - TNF-α directly regulates AMPA receptor trafficking
 *    - Bidirectional control: low TNF-α increases scaling, high TNF-α decreases it
 *    - Essential for homeostatic synaptic plasticity
 *    - Reference: Stellwagen & Malenka (2006) "Synaptic scaling mediated by glial TNF-α"
 *
 * 2. IL-1β and Intrinsic Excitability:
 *    - IL-1β affects voltage-gated channels
 *    - Alters firing threshold and input resistance
 *    - Disrupts homeostatic set points
 *    - Reference: Vezzani & Viviani (2015) "Neuromodulatory properties of inflammatory cytokines"
 *
 * 3. Chronic Inflammation and Homeostatic Failure:
 *    - Sustained inflammation disrupts homeostatic mechanisms
 *    - Prevents proper synaptic scaling
 *    - Leads to hyperexcitability or hypoactivity
 *    - Reference: Galic et al. (2012) "Cytokines and brain excitability"
 *
 * 4. IL-6 and Target Rate Disruption:
 *    - IL-6 shifts homeostatic target firing rates
 *    - Causes maladaptive scaling
 *    - Contributes to excitotoxicity
 *    - Reference: Gruol & Nelson (1997) "Physiological and pathological roles of interleukin-6"
 *
 * 5. Anti-inflammatory Cytokines (IL-10):
 *    - IL-10 restores homeostatic balance
 *    - Normalizes synaptic scaling mechanisms
 *    - Repairs disrupted set points
 *    - Reference: Lim et al. (2013) "Anti-inflammatory effects of IL-10 after stroke"
 *
 * HOMEOSTASIS → IMMUNE PATHWAYS:
 * -------------------------------
 * 1. Hyperexcitability → Immune Activation:
 *    - Excessive neuronal activity triggers inflammation
 *    - Seizures activate microglia and cytokine release
 *    - Homeostatic failure signals immune system
 *    - Reference: Vezzani et al. (2011) "The role of inflammation in epilepsy"
 *
 * 2. Synaptic Instability → Cytokine Release:
 *    - Failed homeostatic scaling triggers immune response
 *    - Unstable networks activate inflammatory pathways
 *    - Danger-associated molecular patterns (DAMPs)
 *    - Reference: Maroso et al. (2010) "Toll-like receptor 4 and high-mobility group box-1"
 *
 * 3. Homeostatic Recovery → IL-10 Release:
 *    - Successful scaling triggers anti-inflammatory signals
 *    - Stable activity promotes immune resolution
 *    - Positive feedback loop for stability
 *    - Reference: Ekdahl et al. (2009) "Microglial activation and neurogenesis"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║              HOMEOSTATIC PLASTICITY-IMMUNE BRIDGE                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │              IMMUNE → HOMEOSTASIS PATHWAYS                          │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  CYTOKINES   │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ TNF-α → ±0.3 │  ───────┐                                       │  ║
 * ║   │   │ IL-1β → -0.2 │         │                                       │  ║
 * ║   │   │ IL-6  → -0.25│         ├──→ Synaptic Scaling Factor            │  ║
 * ║   │   │ IL-10 → +0.2 │         │    Target Firing Rate                │  ║
 * ║   │   └──────────────┘         │    Intrinsic Excitability            │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │  HOMEOSTATIC MECHANISMS         │                             │  ║
 * ║   │   │  - Scaling factor modulation    │                             │  ║
 * ║   │   │  - Target rate shifts           │                             │  ║
 * ║   │   │  - Threshold adaptation         │                             │  ║
 * ║   │   │  - Setpoint disruption          │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │           HOMEOSTASIS → IMMUNE PATHWAYS                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ HYPEREXCITABLE   │ ──→ Inflammation Trigger                    │  ║
 * ║   │   │ SCALING FAILURE  │ ──→ Cytokine Release                        │  ║
 * ║   │   │ INSTABILITY      │ ──→ Immune Activation                       │  ║
 * ║   │   └──────────────────┘                                             │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────────┐                                             │  ║
 * ║   │   │ STABLE SCALING   │ ──→ IL-10 Release                           │  ║
 * ║   │   │ RECOVERY         │ ──→ Immune Resolution                       │  ║
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

#ifndef NIMCP_HOMEOSTATIC_IMMUNE_BRIDGE_H
#define NIMCP_HOMEOSTATIC_IMMUNE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "cognitive/immune/nimcp_brain_immune.h"
#include "plasticity/homeostatic/nimcp_homeostatic.h"

/* Bio-async integration */
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Cytokine homeostatic impact factors (scaling modulation) */
#define CYTOKINE_TNF_SCALING_IMPACT       0.3f   /**< TNF-α bidirectional scaling effect */
#define CYTOKINE_IL1_THRESHOLD_IMPACT    -0.2f   /**< IL-1β → threshold increase */
#define CYTOKINE_IL6_TARGET_IMPACT       -0.25f  /**< IL-6 → target rate decrease */
#define CYTOKINE_IFN_GAMMA_SCALING_IMPACT -0.15f /**< IFN-γ → scaling disruption */
#define CYTOKINE_IL10_RESTORATION         0.2f   /**< IL-10 → homeostatic restoration */

/* Inflammation-homeostasis mapping */
#define INFLAMMATION_SCALING_DISRUPTION_THRESHOLD  0.5f  /**< Inflammation level for scaling disruption */
#define INFLAMMATION_SETPOINT_SHIFT_MAX            0.3f  /**< Maximum setpoint shift from inflammation */

/* Homeostatic instability immune trigger thresholds */
#define INSTABILITY_IMMUNE_TRIGGER_THRESHOLD       0.7f  /**< Instability level to trigger immune */
#define HYPEREXCITABILITY_CYTOKINE_MULTIPLIER      1.8f  /**< Hyperexcitability amplifies cytokine release */

/* Chronic inflammation duration (seconds) */
#define CHRONIC_INFLAMMATION_HOMEOSTATIC_THRESHOLD (86400.0f * 3)  /**< 3 days = chronic for homeostasis */

/* TNF-α biphasic effect levels */
#define TNF_LOW_THRESHOLD    0.3f  /**< Below: increases scaling */
#define TNF_HIGH_THRESHOLD   0.7f  /**< Above: decreases scaling */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Cytokine homeostatic effects
 *
 * Represents how cytokine levels modulate homeostatic plasticity
 */
typedef struct {
    /* Pro-inflammatory effects */
    float tnf_scaling_modulation;      /**< TNF-α effect on scaling (biphasic) */
    float il1_threshold_shift;         /**< IL-1β induced threshold change */
    float il6_target_rate_shift;       /**< IL-6 induced target rate change */
    float ifn_gamma_scaling_disruption;/**< IFN-γ scaling disruption */

    /* Anti-inflammatory effects */
    float il10_restoration_factor;     /**< IL-10 homeostatic restoration */

    /* Aggregate effects */
    float total_scaling_factor_shift;  /**< Combined scaling modulation */
    float total_target_rate_shift;     /**< Combined target rate modulation */
    float total_threshold_shift;       /**< Combined threshold modulation */
    float homeostatic_disruption_level;/**< Overall disruption [0-1] */
} cytokine_homeostatic_effects_t;

/**
 * @brief Inflammation homeostatic state
 *
 * How chronic inflammation affects homeostatic mechanisms
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< >= 3 days */

    /* Homeostatic impacts */
    float scaling_disruption;          /**< Scaling mechanism impairment [0-1] */
    float setpoint_shift;              /**< Target rate/threshold shift [0-1] */
    float adaptation_impairment;       /**< Reduced homeostatic adaptation [0-1] */
    float excitability_dysregulation;  /**< Abnormal excitability [0-1] */

    /* Stability metrics */
    float stability_loss;              /**< Lost homeostatic stability [0-1] */
    bool homeostatic_failure;          /**< Critical homeostasis failure */
} inflammation_homeostatic_state_t;

/**
 * @brief Homeostatic instability immune response
 *
 * How homeostatic failures trigger immune activity
 */
typedef struct {
    /* Instability indicators */
    float hyperexcitability_level;     /**< Excessive firing rate [0-1] */
    float scaling_failure_level;       /**< Failed scaling attempts [0-1] */
    float instability_score;           /**< Overall instability [0-1] */

    /* Immune triggers */
    bool cytokine_triggered;           /**< Immune response activated */
    bool inflammation_triggered;       /**< Inflammation initiated */
    float immune_activation_strength;  /**< Activation level [0-1] */

    /* Failure tracking */
    uint32_t consecutive_failures;     /**< Consecutive scaling failures */
    float failure_duration_sec;        /**< How long unstable */
} homeostatic_immune_trigger_t;

/**
 * @brief Homeostatic recovery immune enhancement
 *
 * How successful homeostasis boosts immune resolution
 */
typedef struct {
    /* Recovery state */
    float stability_improvement;       /**< Stability gain [0-1] */
    float scaling_success_rate;        /**< Successful scaling ratio [0-1] */
    bool achieved_homeostasis;         /**< Reached stable state */

    /* Immune benefits */
    float il10_release_boost;          /**< Anti-inflammatory boost */
    float inflammation_reduction;      /**< Reduced inflammation [0-1] */
    float immune_resolution_speed;     /**< Faster resolution multiplier */
} homeostatic_recovery_immune_boost_t;

/**
 * @brief TNF-α biphasic effect state
 *
 * Models the bidirectional effect of TNF-α on synaptic scaling
 */
typedef struct {
    float tnf_concentration;           /**< Current TNF-α level [0-1] */
    float scaling_enhancement;         /**< Low TNF-α: positive scaling */
    float scaling_suppression;         /**< High TNF-α: negative scaling */
    float net_effect;                  /**< Combined biphasic effect */
    bool in_optimal_range;             /**< TNF-α in beneficial range */
} tnf_biphasic_state_t;

/**
 * @brief Complete homeostatic-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    homeostatic_controller_t homeostatic_controller;

    /* Current state */
    cytokine_homeostatic_effects_t cytokine_effects;
    inflammation_homeostatic_state_t inflammation_state;
    homeostatic_immune_trigger_t instability_trigger;
    homeostatic_recovery_immune_boost_t recovery_boost;
    tnf_biphasic_state_t tnf_state;

    /* Homeostatic parameters (modulated by immune) */
    float base_scaling_factor;         /**< Original scaling factor */
    float base_target_rate;            /**< Original target firing rate */
    float base_threshold;              /**< Original firing threshold */
    float current_scaling_factor;      /**< Immune-modulated scaling */
    float current_target_rate;         /**< Immune-modulated target */
    float current_threshold;           /**< Immune-modulated threshold */

    /* Integration flags */
    bool enable_cytokine_homeostasis_modulation;
    bool enable_inflammation_disruption;
    bool enable_instability_immune_trigger;
    bool enable_recovery_immune_boost;
    bool enable_tnf_biphasic_effect;

    /* Statistics */
    uint64_t total_updates;
    uint32_t cytokine_modulations;
    uint32_t instability_triggered_responses;
    uint32_t recovery_boosts;
    uint32_t homeostatic_failures;
    uint32_t successful_restorations;
    } homeostatic_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_cytokine_homeostasis_modulation;
    bool enable_inflammation_disruption;
    bool enable_instability_immune_trigger;
    bool enable_recovery_immune_boost;
    bool enable_tnf_biphasic_effect;

    /* Sensitivity tuning */
    float cytokine_sensitivity;        /**< Cytokine effect multiplier [0.5-2.0] */
    float inflammation_sensitivity;    /**< Inflammation effect multiplier [0.5-2.0] */
    float instability_trigger_sensitivity; /**< Instability trigger multiplier [0.5-2.0] */

    /* Thresholds */
    float instability_trigger_threshold;     /**< Instability to trigger immune [0.5-0.9] */
    float inflammation_disruption_threshold; /**< Inflammation for disruption [0.3-0.7] */

    /* Homeostatic baseline parameters */
    float baseline_scaling_factor;     /**< Baseline scaling factor */
    float baseline_target_rate;        /**< Baseline target firing rate (Hz) */
    float baseline_threshold;          /**< Baseline firing threshold */
} homeostatic_immune_config_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biological defaults
 * HOW:  Return struct with evidence-based parameters
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int homeostatic_immune_default_config(homeostatic_immune_config_t* config);

/**
 * @brief Create homeostatic-immune bridge
 *
 * WHAT: Initialize bidirectional homeostatic-immune integration
 * WHY:  Enable realistic immune-homeostasis coupling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param homeostatic_controller Homeostatic controller
 * @return New bridge or NULL on failure
 */
homeostatic_immune_bridge_t* homeostatic_immune_bridge_create(
    const homeostatic_immune_config_t* config,
    brain_immune_system_t* immune_system,
    homeostatic_controller_t homeostatic_controller
);

/**
 * @brief Destroy homeostatic-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void homeostatic_immune_bridge_destroy(homeostatic_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Homeostasis API
 * ============================================================================ */

/**
 * @brief Apply cytokine effects to homeostatic parameters
 *
 * WHAT: Modulate homeostatic setpoints based on cytokine levels
 * WHY:  TNF-α regulates scaling, IL-1β affects thresholds, IL-6 shifts targets
 * HOW:  Query immune system cytokines, adjust homeostatic parameters
 *
 * @param bridge Homeostatic-immune bridge
 * @return 0 on success
 */
int homeostatic_immune_apply_cytokine_effects(homeostatic_immune_bridge_t* bridge);

/**
 * @brief Apply chronic inflammation to homeostatic mechanisms
 *
 * WHAT: Disrupt homeostatic scaling and setpoints from prolonged inflammation
 * WHY:  Chronic inflammation causes homeostatic failure
 * HOW:  Check inflammation duration/level, disrupt scaling and adaptation
 *
 * @param bridge Homeostatic-immune bridge
 * @return 0 on success
 */
int homeostatic_immune_apply_inflammation_effects(homeostatic_immune_bridge_t* bridge);

/**
 * @brief Compute TNF-α biphasic effect on scaling
 *
 * WHAT: Calculate bidirectional TNF-α effect (low enhances, high suppresses)
 * WHY:  TNF-α has U-shaped dose-response for synaptic scaling
 * HOW:  Low TNF-α → positive scaling, high TNF-α → negative scaling
 *
 * @param bridge Homeostatic-immune bridge
 * @param tnf_level Current TNF-α concentration [0-1]
 * @return Net scaling factor modulation
 */
float homeostatic_immune_compute_tnf_biphasic(
    const homeostatic_immune_bridge_t* bridge,
    float tnf_level
);

/**
 * @brief Apply immune-modulated homeostatic parameters
 *
 * WHAT: Update homeostatic controller with immune-adjusted parameters
 * WHY:  Implement cytokine effects on synaptic scaling and excitability
 * HOW:  Set modulated scaling factor, target rate, threshold in controller
 *
 * @param bridge Homeostatic-immune bridge
 * @return 0 on success
 */
int homeostatic_immune_apply_modulated_parameters(homeostatic_immune_bridge_t* bridge);

/* ============================================================================
 * Homeostasis → Immune API
 * ============================================================================ */

/**
 * @brief Trigger immune response from homeostatic instability
 *
 * WHAT: Activate immune system from hyperexcitability or scaling failure
 * WHY:  Homeostatic failure signals network danger
 * HOW:  Check instability level, trigger cytokine release and inflammation
 *
 * @param bridge Homeostatic-immune bridge
 * @param firing_rates Current neuron firing rates
 * @param num_neurons Number of neurons
 * @return 0 on success
 */
int homeostatic_immune_trigger_from_instability(
    homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons
);

/**
 * @brief Detect hyperexcitability
 *
 * WHAT: Identify excessive neuronal firing rates
 * WHY:  Hyperexcitability triggers inflammatory response
 * HOW:  Check firing rates against target, compute deviation
 *
 * @param bridge Homeostatic-immune bridge
 * @param firing_rates Current firing rates
 * @param num_neurons Number of neurons
 * @return Hyperexcitability level [0-1]
 */
float homeostatic_immune_detect_hyperexcitability(
    const homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons
);

/**
 * @brief Detect scaling failure
 *
 * WHAT: Identify failed homeostatic scaling attempts
 * WHY:  Scaling failure indicates homeostatic breakdown
 * HOW:  Track consecutive failures, compute failure rate
 *
 * @param bridge Homeostatic-immune bridge
 * @param scaling_success True if last scaling succeeded
 * @return Failure level [0-1]
 */
float homeostatic_immune_detect_scaling_failure(
    homeostatic_immune_bridge_t* bridge,
    bool scaling_success
);

/**
 * @brief Boost immune resolution from homeostatic recovery
 *
 * WHAT: Enhance immunity from successful homeostatic stabilization
 * WHY:  Stable networks promote anti-inflammatory state
 * HOW:  Check stability, release IL-10, reduce inflammation
 *
 * @param bridge Homeostatic-immune bridge
 * @param is_stable True if homeostasis achieved
 * @return 0 on success
 */
int homeostatic_immune_boost_from_recovery(
    homeostatic_immune_bridge_t* bridge,
    bool is_stable
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update homeostatic-immune bridge (both directions)
 *
 * WHAT: Process all homeostatic-immune interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, trigger immune from instability, boost from recovery
 *
 * @param bridge Homeostatic-immune bridge
 * @param firing_rates Current neuron firing rates
 * @param num_neurons Number of neurons
 * @param is_stable True if homeostasis is stable
 * @param scaling_success True if last scaling succeeded
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int homeostatic_immune_bridge_update(
    homeostatic_immune_bridge_t* bridge,
    const float* firing_rates,
    uint32_t num_neurons,
    bool is_stable,
    bool scaling_success,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current cytokine homeostatic effects
 *
 * @param bridge Homeostatic-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int homeostatic_immune_get_cytokine_effects(
    const homeostatic_immune_bridge_t* bridge,
    cytokine_homeostatic_effects_t* effects
);

/**
 * @brief Get current inflammation homeostatic state
 *
 * @param bridge Homeostatic-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int homeostatic_immune_get_inflammation_state(
    const homeostatic_immune_bridge_t* bridge,
    inflammation_homeostatic_state_t* state
);

/**
 * @brief Check if experiencing homeostatic failure
 *
 * WHAT: Determine if homeostatic mechanisms have failed
 * WHY:  Failure state is critical for immune intervention
 * HOW:  Check disruption level and failure flag
 *
 * @param bridge Homeostatic-immune bridge
 * @return true if experiencing homeostatic failure
 */
bool homeostatic_immune_is_homeostatic_failure(
    const homeostatic_immune_bridge_t* bridge
);

/**
 * @brief Get current scaling factor (immune-modulated)
 *
 * @param bridge Homeostatic-immune bridge
 * @return Current scaling factor
 */
float homeostatic_immune_get_current_scaling_factor(
    const homeostatic_immune_bridge_t* bridge
);

/**
 * @brief Get current target rate (immune-modulated)
 *
 * @param bridge Homeostatic-immune bridge
 * @return Current target firing rate (Hz)
 */
float homeostatic_immune_get_current_target_rate(
    const homeostatic_immune_bridge_t* bridge
);

/**
 * @brief Get current threshold (immune-modulated)
 *
 * @param bridge Homeostatic-immune bridge
 * @return Current firing threshold
 */
float homeostatic_immune_get_current_threshold(
    const homeostatic_immune_bridge_t* bridge
);

/**
 * @brief Get homeostatic disruption level
 *
 * @param bridge Homeostatic-immune bridge
 * @return Disruption level [0-1]
 */
float homeostatic_immune_get_disruption_level(
    const homeostatic_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_HOMEOSTATIC
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int homeostatic_immune_connect_bio_async(homeostatic_immune_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister bridge from bio-async
 * WHY:  Clean shutdown of messaging
 * HOW:  Unregister from bio_router
 *
 * @param bridge Bridge instance
 * @return 0 on success
 */
int homeostatic_immune_disconnect_bio_async(homeostatic_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool homeostatic_immune_is_bio_async_connected(const homeostatic_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HOMEOSTATIC_IMMUNE_BRIDGE_H */
