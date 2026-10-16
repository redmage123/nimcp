/**
 * @file nimcp_synaptic_scaling_immune_bridge.h
 * @brief Synaptic Scaling-Immune System Integration Bridge
 * @version 1.0.0
 * @date 2025-12-11
 *
 * WHAT: Bidirectional integration between brain immune system and synaptic scaling mechanisms
 * WHY:  Biological evidence shows TNF-α and cytokines directly regulate synaptic scaling and
 *       AMPA receptor trafficking. Essential for realistic synaptic homeostasis modeling.
 * HOW:  TNF-α modulates scaling factors, inflammation affects scaling rate, aberrant scaling
 *       triggers immune responses, recovery restores normal scaling dynamics.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * IMMUNE → SYNAPTIC SCALING PATHWAYS:
 * -----------------------------------
 * 1. TNF-α Regulation of Synaptic Scaling:
 *    - Glial TNF-α increases AMPA receptor surface expression
 *    - Acts as gliotransmitter to enhance excitatory synaptic strength
 *    - Required for homeostatic scaling up of synaptic weights
 *    - Direct receptor trafficking modulation (Stellwagen & Malenka, 2006)
 *    - Reference: Stellwagen & Malenka (2006) "Synaptic scaling mediated by glial TNF-α"
 *
 * 2. IL-1β Effects on Synaptic Strength:
 *    - Modulates NMDA receptor function
 *    - Affects long-term potentiation threshold
 *    - Influences synaptic plasticity bidirectionally
 *    - Reference: Goshen et al. (2007) "A dual role for IL-1 in hippocampal plasticity"
 *
 * 3. Chronic Inflammation and Aberrant Scaling:
 *    - Sustained TNF-α leads to excessive scaling
 *    - Impaired homeostatic mechanisms
 *    - Runaway excitation or suppression
 *    - Network instability
 *    - Reference: Beattie et al. (2002) "Control of synaptic strength by glial TNFα"
 *
 * 4. IL-10 Restoration of Normal Scaling:
 *    - Anti-inflammatory cytokines restore homeostasis
 *    - Normalize AMPA receptor trafficking
 *    - Re-establish stable scaling dynamics
 *    - Reference: Stellwagen et al. (2005) "Differential regulation of AMPA receptor trafficking"
 *
 * SYNAPTIC SCALING → IMMUNE PATHWAYS:
 * -----------------------------------
 * 1. Aberrant Scaling Detection:
 *    - Excessive scaling up → over-excitation → immune alert
 *    - Excessive scaling down → hypoactivity → immune alert
 *    - Rapid scaling oscillations → instability → immune response
 *    - Scaling failure → homeostatic breakdown → immune activation
 *
 * 2. Network Instability Triggers:
 *    - Runaway excitation (seizure-like) → pro-inflammatory response
 *    - Global silencing → metabolic stress → immune activation
 *    - Loss of E/I balance → immune system intervention
 *
 * 3. Recovery Signals:
 *    - Restoration of normal scaling → IL-10 release
 *    - Homeostatic stability achieved → anti-inflammatory state
 *    - Balanced network activity → immune resolution
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║               SYNAPTIC SCALING-IMMUNE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │             IMMUNE → SYNAPTIC SCALING PATHWAYS                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  TNF-α       │                                                 │  ║
 * ║   │   │ ──────────── │                                                 │  ║
 * ║   │   │ High TNF-α   │  ───────┐                                       │  ║
 * ║   │   │   ↑          │         │                                       │  ║
 * ║   │   │ Scale Up     │         ├──→ Scaling Factor Modulation          │  ║
 * ║   │   │   (2-3x)     │         │    (AMPA receptor trafficking)        │  ║
 * ║   │   └──────────────┘         │                                       │  ║
 * ║   │                            ▼                                       │  ║
 * ║   │   ┌─────────────────────────────────┐                             │  ║
 * ║   │   │   SYNAPTIC SCALING SYSTEM       │                             │  ║
 * ║   │   │  - Scaling factor modulation    │                             │  ║
 * ║   │   │  - Scaling rate adjustment      │                             │  ║
 * ║   │   │  - AMPA receptor density        │                             │  ║
 * ║   │   │  - Homeostatic set points       │                             │  ║
 * ║   │   └─────────────────────────────────┘                             │  ║
 * ║   │                            ▲                                       │  ║
 * ║   │   ┌──────────────┐         │                                       │  ║
 * ║   │   │   IL-10      │         │                                       │  ║
 * ║   │   │ Anti-inflam  │  ───────┘                                       │  ║
 * ║   │   │   Restores   │     Normalizes Scaling                          │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │          SYNAPTIC SCALING → IMMUNE PATHWAYS                         │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  ABERRANT    │ ──→ Pro-inflammatory Trigger                    │  ║
 * ║   │   │  SCALING     │     (IL-1β, IL-6, TNF-α)                        │  ║
 * ║   │   │  - Over-exc  │ ──→ Immune Alert                                │  ║
 * ║   │   │  - Hypoact   │                                                 │  ║
 * ║   │   │  - Unstable  │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   ┌──────────────┐                                                 │  ║
 * ║   │   │  NORMAL      │ ──→ Anti-inflammatory Release                   │  ║
 * ║   │   │  SCALING     │     (IL-10)                                     │  ║
 * ║   │   │  RESTORED    │                                                 │  ║
 * ║   │   └──────────────┘                                                 │  ║
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

#ifndef NIMCP_SYNAPTIC_SCALING_IMMUNE_BRIDGE_H
#define NIMCP_SYNAPTIC_SCALING_IMMUNE_BRIDGE_H

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

/* TNF-α scaling modulation factors */
#define TNF_ALPHA_SCALING_BASELINE       1.0f   /**< No TNF-α effect */
#define TNF_ALPHA_SCALING_LOW_BOOST      1.2f   /**< Low TNF-α (physiological) */
#define TNF_ALPHA_SCALING_MED_BOOST      1.8f   /**< Medium TNF-α (moderate inflammation) */
#define TNF_ALPHA_SCALING_HIGH_BOOST     2.5f   /**< High TNF-α (strong inflammation) */
#define TNF_ALPHA_SCALING_EXCESSIVE      3.5f   /**< Excessive TNF-α (pathological) */

/* IL-1β plasticity threshold modulation */
#define IL1_BETA_LTP_THRESHOLD_NORMAL    1.0f   /**< Normal LTP threshold */
#define IL1_BETA_LTP_THRESHOLD_ELEVATED  1.3f   /**< IL-1β elevates threshold */
#define IL1_BETA_LTP_THRESHOLD_HIGH      1.6f   /**< High IL-1β strongly elevates */

/* Scaling rate modulation by inflammation */
#define INFLAMMATION_SCALING_RATE_NONE       1.0f   /**< No inflammation */
#define INFLAMMATION_SCALING_RATE_LOCAL      1.1f   /**< Local inflammation */
#define INFLAMMATION_SCALING_RATE_REGIONAL   1.3f   /**< Regional inflammation */
#define INFLAMMATION_SCALING_RATE_SYSTEMIC   1.8f   /**< Systemic inflammation */
#define INFLAMMATION_SCALING_RATE_STORM      2.5f   /**< Cytokine storm (aberrant) */

/* Aberrant scaling detection thresholds */
#define SCALING_ABERRANT_THRESHOLD           2.5f   /**< Factor above which scaling is aberrant */
#define SCALING_INSTABILITY_THRESHOLD        0.15f  /**< Variance threshold for instability */
#define SCALING_HYPOACTIVITY_THRESHOLD       0.3f   /**< Factor below which scaling is hypoactive */

/* Recovery thresholds */
#define SCALING_RECOVERY_THRESHOLD           1.2f   /**< Near-normal scaling factor */
#define SCALING_STABILITY_DURATION_SEC       300.0f /**< 5 minutes stable = recovery */

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief TNF-α synaptic scaling effects
 *
 * Represents how TNF-α concentration modulates synaptic scaling
 */
typedef struct {
    /* TNF-α levels and effects */
    float tnf_alpha_concentration;     /**< Current TNF-α level [0-1] */
    float scaling_factor_modulation;   /**< TNF-α induced scaling boost [1.0-3.5x] */
    float ampa_receptor_trafficking;   /**< AMPA receptor insertion rate [0-1] */

    /* Receptor regulation */
    float receptor_surface_density;    /**< Surface AMPA receptor density [0-1] */
    float receptor_insertion_rate;     /**< Rate of receptor insertion */
    float receptor_internalization_rate; /**< Rate of receptor removal */

    /* Scaling dynamics */
    float effective_scaling_rate;      /**< TNF-α modulated scaling rate */
    float homeostatic_set_point;       /**< Target activity level (TNF-α shifts this) */
} tnf_alpha_scaling_effects_t;

/**
 * @brief IL-1β plasticity threshold effects
 *
 * How IL-1β modulates plasticity thresholds
 */
typedef struct {
    float il1_beta_concentration;      /**< Current IL-1β level [0-1] */
    float ltp_threshold_modulation;    /**< LTP threshold shift [1.0-1.6x] */
    float ltd_threshold_modulation;    /**< LTD threshold shift */
    float plasticity_rate_modulation;  /**< Overall plasticity rate change */
} il1_beta_plasticity_effects_t;

/**
 * @brief Inflammation scaling state
 *
 * How chronic inflammation affects scaling mechanisms
 */
typedef struct {
    /* Inflammation state */
    brain_inflammation_level_t current_level;
    float inflammation_duration_sec;   /**< How long inflamed */
    bool is_chronic;                   /**< Chronic inflammation (>30 min) */

    /* Scaling impacts */
    float scaling_rate_multiplier;     /**< Inflammation accelerates scaling */
    float scaling_aberrance_risk;      /**< Risk of pathological scaling [0-1] */
    float homeostatic_impairment;      /**< Degree of homeostatic failure [0-1] */

    /* Network stability */
    float excitation_inhibition_balance; /**< E/I balance [0=all I, 1=all E] */
    float network_stability;           /**< Network stability measure [0-1] */
    bool runaway_excitation;           /**< Runaway excitation detected */
    bool global_silencing;             /**< Global silencing detected */
} inflammation_scaling_state_t;

/**
 * @brief Aberrant scaling detection
 *
 * Monitors for pathological scaling patterns
 */
typedef struct {
    /* Aberrance indicators */
    bool excessive_scale_up;           /**< Scaling factor too high */
    bool excessive_scale_down;         /**< Scaling factor too low */
    bool scaling_oscillations;         /**< Rapid oscillations detected */
    bool homeostatic_failure;          /**< Homeostasis not converging */

    /* Measurements */
    float scaling_factor_mean;         /**< Mean scaling factor over window */
    float scaling_factor_variance;     /**< Variance (instability measure) */
    float convergence_rate;            /**< Rate of approach to target */
    float time_since_last_stable_sec;  /**< Time since stability */

    /* Immune trigger state */
    bool immune_triggered;             /**< Immune response triggered */
    uint32_t trigger_count;            /**< Number of times triggered */
    float severity;                    /**< Aberrance severity [0-1] */
} aberrant_scaling_detection_t;

/**
 * @brief Scaling recovery state
 *
 * Tracks recovery of normal scaling after immune intervention
 */
typedef struct {
    /* Recovery progress */
    bool in_recovery;                  /**< Currently recovering */
    float recovery_progress;           /**< Progress toward normal [0-1] */
    float time_stable_sec;             /**< Time at normal scaling */

    /* Recovery metrics */
    float baseline_scaling_factor;     /**< Pre-aberrance baseline */
    float current_scaling_factor;      /**< Current factor */
    float target_scaling_factor;       /**< Recovery target */

    /* IL-10 release state */
    float il10_release_rate;           /**< Anti-inflammatory release */
    bool recovery_complete;            /**< Full recovery achieved */
} scaling_recovery_state_t;

/**
 * @brief Complete synaptic scaling-immune bridge state
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    /* System handles */
    brain_immune_system_t* immune_system;
    homeostatic_controller_t homeostatic_controller;

    /* Current state */
    tnf_alpha_scaling_effects_t tnf_effects;
    il1_beta_plasticity_effects_t il1_effects;
    inflammation_scaling_state_t inflammation_state;
    aberrant_scaling_detection_t aberrance;
    scaling_recovery_state_t recovery;

    /* Integration flags */
    bool enable_tnf_scaling_modulation;
    bool enable_il1_threshold_modulation;
    bool enable_inflammation_rate_modulation;
    bool enable_aberrance_detection;
    bool enable_recovery_tracking;
    bool enable_il10_restoration;

    /* Configuration */
    float tnf_sensitivity;             /**< TNF-α effect sensitivity [0.5-2.0] */
    float aberrance_sensitivity;       /**< Aberrance detection sensitivity */
    float recovery_threshold;          /**< Threshold for recovery [0.8-1.2] */

    /* Statistics */
    uint64_t total_updates;
    uint32_t tnf_modulations;
    uint32_t aberrance_detections;
    uint32_t immune_triggers;
    uint32_t recoveries_completed;
    } synaptic_scaling_immune_bridge_t;

/**
 * @brief Bridge configuration
 */
typedef struct {
    /* Feature enables */
    bool enable_tnf_scaling_modulation;
    bool enable_il1_threshold_modulation;
    bool enable_inflammation_rate_modulation;
    bool enable_aberrance_detection;
    bool enable_recovery_tracking;
    bool enable_il10_restoration;

    /* Sensitivity tuning */
    float tnf_sensitivity;             /**< TNF-α effect multiplier [0.5-2.0] */
    float aberrance_sensitivity;       /**< Detection sensitivity [0.5-2.0] */
    float recovery_threshold;          /**< Recovery threshold [0.8-1.2] */

    /* Detection thresholds */
    float aberrant_scale_up_threshold; /**< Factor for excessive scale-up */
    float aberrant_scale_down_threshold; /**< Factor for excessive scale-down */
    float instability_variance_threshold; /**< Variance for oscillation detection */
} synaptic_scaling_immune_config_t;

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
int synaptic_scaling_immune_default_config(synaptic_scaling_immune_config_t* config);

/**
 * @brief Create synaptic scaling-immune bridge
 *
 * WHAT: Initialize bidirectional synaptic scaling-immune integration
 * WHY:  Enable realistic TNF-α mediated synaptic scaling
 * HOW:  Allocate structure, link subsystems
 *
 * @param config Configuration (NULL for defaults)
 * @param immune_system Brain immune system
 * @param homeostatic_controller Homeostatic controller (optional, can be NULL)
 * @return New bridge or NULL on failure
 */
synaptic_scaling_immune_bridge_t* synaptic_scaling_immune_bridge_create(
    const synaptic_scaling_immune_config_t* config,
    brain_immune_system_t* immune_system,
    homeostatic_controller_t homeostatic_controller
);

/**
 * @brief Destroy synaptic scaling-immune bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Free structure (doesn't destroy linked systems)
 *
 * @param bridge Bridge to destroy
 */
void synaptic_scaling_immune_bridge_destroy(synaptic_scaling_immune_bridge_t* bridge);

/* ============================================================================
 * Immune → Synaptic Scaling API
 * ============================================================================ */

/**
 * @brief Apply TNF-α effects to synaptic scaling
 *
 * WHAT: Modulate scaling factor based on TNF-α concentration
 * WHY:  TNF-α directly regulates AMPA receptor trafficking
 * HOW:  Query immune TNF-α level, adjust scaling factor and rate
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_apply_tnf_effects(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Apply IL-1β effects to plasticity thresholds
 *
 * WHAT: Modulate LTP/LTD thresholds based on IL-1β
 * WHY:  IL-1β affects NMDA receptor function and plasticity
 * HOW:  Query immune IL-1β, adjust plasticity thresholds
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_apply_il1_effects(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Apply inflammation effects to scaling rate
 *
 * WHAT: Accelerate or impair scaling based on inflammation
 * WHY:  Chronic inflammation disrupts homeostatic mechanisms
 * HOW:  Check inflammation level, adjust scaling rate
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_apply_inflammation_effects(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Compute TNF-α scaling factor modulation
 *
 * WHAT: Calculate scaling factor boost from TNF-α
 * WHY:  TNF-α enhances AMPA receptor surface expression
 * HOW:  Map TNF-α concentration to scaling multiplier [1.0-3.5x]
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return Scaling factor modulation [1.0-3.5]
 */
float synaptic_scaling_immune_compute_tnf_modulation(
    const synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Restore normal scaling from IL-10
 *
 * WHAT: Normalize scaling parameters from anti-inflammatory state
 * WHY:  IL-10 restores normal AMPA receptor trafficking
 * HOW:  Query immune IL-10 level, restore baseline scaling
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_restore_from_il10(
    synaptic_scaling_immune_bridge_t* bridge
);

/* ============================================================================
 * Synaptic Scaling → Immune API
 * ============================================================================ */

/**
 * @brief Detect aberrant scaling patterns
 *
 * WHAT: Monitor for pathological scaling (excessive, oscillating, failing)
 * WHY:  Aberrant scaling indicates network instability requiring immune response
 * HOW:  Check scaling factor magnitude, variance, convergence rate
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_detect_aberrance(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Trigger immune response from aberrant scaling
 *
 * WHAT: Activate immune system from pathological scaling
 * WHY:  Aberrant scaling threatens network stability
 * HOW:  Present antigen to immune, trigger pro-inflammatory response
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_trigger_from_aberrance(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Signal recovery from scaling restoration
 *
 * WHAT: Release IL-10 when normal scaling restored
 * WHY:  Homeostatic stability achieved, anti-inflammatory appropriate
 * HOW:  Check stability duration, release IL-10 if stable
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return 0 on success
 */
int synaptic_scaling_immune_signal_recovery(
    synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Check for runaway excitation
 *
 * WHAT: Detect runaway excitation from over-scaling
 * WHY:  Runaway excitation is emergency requiring immune intervention
 * HOW:  Monitor E/I balance, scaling factor, network activity
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return true if runaway excitation detected
 */
bool synaptic_scaling_immune_check_runaway_excitation(
    const synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Check for global silencing
 *
 * WHAT: Detect global network silencing from under-scaling
 * WHY:  Global silencing indicates metabolic stress
 * HOW:  Monitor scaling factor, activity levels
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return true if global silencing detected
 */
bool synaptic_scaling_immune_check_global_silencing(
    const synaptic_scaling_immune_bridge_t* bridge
);

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

/**
 * @brief Update synaptic scaling-immune bridge (both directions)
 *
 * WHAT: Process all immune-scaling interactions
 * WHY:  Advance coupled state machine
 * HOW:  Apply cytokine effects, detect aberrance, track recovery
 *
 * @param bridge Synaptic scaling-immune bridge
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int synaptic_scaling_immune_bridge_update(
    synaptic_scaling_immune_bridge_t* bridge,
    uint64_t delta_ms
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current TNF-α scaling effects
 *
 * @param bridge Synaptic scaling-immune bridge
 * @param effects Output effects structure
 * @return 0 on success
 */
int synaptic_scaling_immune_get_tnf_effects(
    const synaptic_scaling_immune_bridge_t* bridge,
    tnf_alpha_scaling_effects_t* effects
);

/**
 * @brief Get current inflammation scaling state
 *
 * @param bridge Synaptic scaling-immune bridge
 * @param state Output state structure
 * @return 0 on success
 */
int synaptic_scaling_immune_get_inflammation_state(
    const synaptic_scaling_immune_bridge_t* bridge,
    inflammation_scaling_state_t* state
);

/**
 * @brief Check if scaling is aberrant
 *
 * WHAT: Determine if scaling is pathological
 * WHY:  Aberrance requires immune intervention
 * HOW:  Check aberrance detection flags and severity
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return true if scaling is aberrant
 */
bool synaptic_scaling_immune_is_aberrant(
    const synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Get effective scaling factor
 *
 * WHAT: Get TNF-α and inflammation modulated scaling factor
 * WHY:  This is the actual factor applied to synaptic weights
 * HOW:  Combine baseline, TNF-α modulation, inflammation effects
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return Effective scaling factor
 */
float synaptic_scaling_immune_get_effective_scaling_factor(
    const synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Get AMPA receptor surface density
 *
 * WHAT: Get current AMPA receptor density (TNF-α modulated)
 * WHY:  Receptor density determines synaptic strength
 * HOW:  Return TNF-α modulated receptor density
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return AMPA receptor surface density [0-1]
 */
float synaptic_scaling_immune_get_ampa_density(
    const synaptic_scaling_immune_bridge_t* bridge
);

/**
 * @brief Get recovery progress
 *
 * @param bridge Synaptic scaling-immune bridge
 * @return Recovery progress [0-1]
 */
float synaptic_scaling_immune_get_recovery_progress(
    const synaptic_scaling_immune_bridge_t* bridge
);


/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect bridge to bio-async router
 *
 * WHAT: Register bridge as bio-async module
 * WHY:  Enable inter-module messaging for distributed immune signals
 * HOW:  Register with bio_router using BIO_MODULE_IMMUNE_SYNAPTIC_SCALING
 *
 * @param bridge Bridge instance
 * @return 0 on success, -1 on error
 */
int synaptic_scaling_immune_connect_bio_async(synaptic_scaling_immune_bridge_t* bridge);

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
int synaptic_scaling_immune_disconnect_bio_async(synaptic_scaling_immune_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge Bridge instance
 * @return true if connected
 */
bool synaptic_scaling_immune_is_bio_async_connected(const synaptic_scaling_immune_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_SYNAPTIC_SCALING_IMMUNE_BRIDGE_H */
