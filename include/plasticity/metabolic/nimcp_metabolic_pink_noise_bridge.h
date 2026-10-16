//=============================================================================
// nimcp_metabolic_pink_noise_bridge.h - Metabolic Plasticity Pink Noise Bridge
//=============================================================================
/**
 * @file nimcp_metabolic_pink_noise_bridge.h
 * @brief Bidirectional integration between metabolic plasticity and pink noise
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Connect pink noise modulation with metabolic energy state
 * WHY:  Biological metabolic processes exhibit stochastic fluctuations:
 *       - ATP levels fluctuate with 1/f characteristics (Mathupala et al., 2006)
 *       - Mitochondrial dynamics are inherently noisy (Zorov et al., 2014)
 *       - Glucose delivery varies stochastically (Gruetter et al., 2003)
 *       - Pink noise models realistic energy supply variations
 *
 * HOW:  Apply 1/f noise to ATP recovery rates and energy thresholds;
 *       Metabolic state modulates noise characteristics.
 *
 * BIOLOGICAL BASIS:
 * =================
 * 1. MITOCHONDRIAL DYNAMICS:
 *    - Mitochondrial ATP production is stochastic (Zorov et al., 2014)
 *    - Fluctuations follow 1/f spectrum due to multi-timescale processes
 *    - Fast: Enzyme kinetics (~ms), Slow: Mitochondrial trafficking (~minutes)
 *    - Reference: Mathupala et al. (2006) "Hexokinase-2 bound to mitochondria"
 *
 * 2. GLUCOSE DELIVERY VARIABILITY:
 *    - Blood flow varies with 1/f characteristics (Bassingthwaighte et al., 1994)
 *    - Astrocyte lactate shuttle has stochastic delays
 *    - Glucose transporter stochasticity (Simpson et al., 2007)
 *    - Pink noise captures multi-timescale delivery dynamics
 *
 * 3. ENERGY HOMEOSTASIS NOISE:
 *    - ATP/ADP ratio fluctuates around homeostatic setpoint
 *    - Fluctuations are autocorrelated (not white noise)
 *    - 1/f noise maintains physiological realism
 *    - Reference: Gruetter et al. (2003) "Direct measurement of brain glucose"
 *
 * 4. METABOLIC UNCERTAINTY:
 *    - Energy availability has inherent unpredictability
 *    - Pink noise models realistic uncertainty
 *    - Prevents overfitting to deterministic energy models
 *    - Critical for robust learning algorithms
 *
 * INTEGRATION PATHWAYS:
 * =====================
 * PINK NOISE → METABOLIC:
 * -----------------------
 * - Recovery rate modulation: recovery_rate *= (1 + pink_noise)
 * - ATP threshold jitter: threshold += pink_noise * jitter_strength
 * - Cost variability: event_cost *= (1 + pink_noise * cost_var)
 * - Delivery stochasticity: glucose_delivery + pink_noise
 *
 * METABOLIC → PINK NOISE:
 * -----------------------
 * - Energy depletion → increased noise amplitude (metabolic stress)
 * - Critical state → shift toward white noise (loss of homeostasis)
 * - Healthy state → restore normal 1/f spectrum
 * - Recovery phase → reduced noise (stable metabolism)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════╗
 * ║              METABOLIC PINK NOISE BRIDGE                               ║
 * ╠═══════════════════════════════════════════════════════════════════════╣
 * ║                                                                        ║
 * ║   ┌─────────────────┐          ┌─────────────────────┐               ║
 * ║   │  Pink Noise     │◄────────►│  Metabolic          │               ║
 * ║   │  Generator      │          │  Plasticity         │               ║
 * ║   └─────────────────┘          └─────────────────────┘               ║
 * ║           │                               │                           ║
 * ║           │ 1/f noise samples             │ ATP level                 ║
 * ║           │                               │ Energy state              ║
 * ║           ▼                               ▼                           ║
 * ║   ┌───────────────────────────────────────────────────┐              ║
 * ║   │         STOCHASTIC MODULATION LAYER                │              ║
 * ║   │                                                    │              ║
 * ║   │  • Recovery rate noise:  ±10-20% variation        │              ║
 * ║   │  • Threshold jitter:     ±5% ATP threshold        │              ║
 * ║   │  • Cost variability:     ±15% event cost          │              ║
 * ║   │  • Delivery fluctuations: Glucose + pink noise    │              ║
 * ║   └───────────────────────────────────────────────────┘              ║
 * ║                                                                        ║
 * ║   ┌───────────────────────────────────────────────────┐              ║
 * ║   │         FEEDBACK ADAPTATION LAYER                  │              ║
 * ║   │                                                    │              ║
 * ║   │  Energy Depleted   → Amplitude *= 1.5             │              ║
 * ║   │  Energy Critical   → Alpha shift toward 0 (white) │              ║
 * ║   │  Energy Healthy    → Restore α = 1.0 (pink)       │              ║
 * ║   │  Recovery Active   → Reduce amplitude             │              ║
 * ║   └───────────────────────────────────────────────────┘              ║
 * ║                                                                        ║
 * ╚═══════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * -----------
 * - Mathupala, S.P., Ko, Y.H., & Pedersen, P.L. (2006). Hexokinase II:
 *   Cancer's double-edged sword acting as both facilitator and gatekeeper
 *   of malignancy. Oncogene 25(34), 4777-4786.
 * - Zorov, D.B., Juhaszova, M., & Sollott, S.J. (2014). Mitochondrial
 *   reactive oxygen species and modulation of cell signaling.
 *   Physiol Rev 94(3), 909-950.
 * - Gruetter, R., et al. (2003). Direct measurement of brain glucose
 *   concentrations in humans by 13C NMR spectroscopy.
 *   Proc Natl Acad Sci 100(20), 11601-11606.
 * - Bassingthwaighte, J.B., Liebovitch, L.S., & West, B.J. (1994).
 *   Fractal Physiology. Oxford University Press.
 * - Simpson, I.A., et al. (2007). Supply and demand in cerebral energy
 *   metabolism: the role of nutrient transporters.
 *   J Cereb Blood Flow Metab 27(11), 1766-1791.
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

#ifndef NIMCP_METABOLIC_PINK_NOISE_BRIDGE_H
#define NIMCP_METABOLIC_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct metabolic_plasticity metabolic_plasticity_t;
typedef struct pink_noise_generator_internal_t* pink_noise_generator_t;

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default noise modulation strengths (biological realism) */
#define METABOLIC_PINK_RECOVERY_NOISE_STRENGTH    0.15f  /**< ±15% recovery variation */
#define METABOLIC_PINK_THRESHOLD_JITTER           0.05f  /**< ±5% threshold jitter */
#define METABOLIC_PINK_COST_VARIABILITY           0.10f  /**< ±10% cost variation */
#define METABOLIC_PINK_DELIVERY_NOISE             0.20f  /**< ±20% delivery fluctuation */

/* Energy-dependent noise scaling */
#define METABOLIC_PINK_DEPLETED_AMP_SCALE         1.5f   /**< 1.5x noise when depleted */
#define METABOLIC_PINK_CRITICAL_AMP_SCALE         2.0f   /**< 2x noise when critical */
#define METABOLIC_PINK_HEALTHY_AMP_SCALE          1.0f   /**< Normal noise when healthy */

/* Alpha (spectral exponent) modulation */
#define METABOLIC_PINK_NORMAL_ALPHA               1.0f   /**< Pink noise (1/f) */
#define METABOLIC_PINK_CRITICAL_ALPHA_SHIFT      -0.3f   /**< Shift toward white (0.7) */
#define METABOLIC_PINK_RECOVERY_ALPHA_SHIFT       0.2f   /**< Shift toward red (1.2) */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Noise modulation targets
 *
 * WHAT: Identifies which metabolic parameter to modulate
 * WHY:  Different parameters have different biological noise characteristics
 */
typedef enum {
    METABOLIC_NOISE_RECOVERY_RATE = 0,  /**< ATP recovery rate */
    METABOLIC_NOISE_LTP_THRESHOLD,      /**< LTP threshold */
    METABOLIC_NOISE_LTD_THRESHOLD,      /**< LTD threshold */
    METABOLIC_NOISE_LTP_COST,           /**< LTP energy cost */
    METABOLIC_NOISE_LTD_COST,           /**< LTD energy cost */
    METABOLIC_NOISE_GLUCOSE_DELIVERY,   /**< Glucose delivery rate */
    METABOLIC_NOISE_TARGET_COUNT
} metabolic_noise_target_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Pink noise modulation configuration
 *
 * Controls how pink noise affects each metabolic parameter
 */
typedef struct {
    float recovery_rate_strength;    /**< Recovery rate noise strength [0-0.5] */
    float threshold_jitter_strength; /**< Threshold jitter strength [0-0.2] */
    float cost_variability_strength; /**< Cost variability strength [0-0.3] */
    float delivery_noise_strength;   /**< Delivery noise strength [0-0.5] */

    /* Energy-state dependent scaling */
    float depleted_amplitude_scale;  /**< Amplitude scale when depleted */
    float critical_amplitude_scale;  /**< Amplitude scale when critical */
    float healthy_amplitude_scale;   /**< Amplitude scale when healthy */

    /* Alpha (spectral exponent) modulation */
    float normal_alpha;              /**< Normal spectral exponent (1.0 = pink) */
    float critical_alpha_shift;      /**< Alpha shift when critical */
    float recovery_alpha_shift;      /**< Alpha shift during recovery */

    /* Feature enables */
    bool enable_recovery_noise;      /**< Enable recovery rate modulation */
    bool enable_threshold_jitter;    /**< Enable threshold jitter */
    bool enable_cost_variability;    /**< Enable cost variability */
    bool enable_delivery_noise;      /**< Enable delivery fluctuations */
    bool enable_adaptive_amplitude;  /**< Adapt amplitude to energy state */
    bool enable_adaptive_alpha;      /**< Adapt alpha to energy state */
} metabolic_pink_noise_config_t;

/**
 * @brief Current noise modulation values
 *
 * Stores most recent noise samples for each target
 */
typedef struct {
    float recovery_rate_noise;       /**< Current recovery rate modulation */
    float ltp_threshold_noise;       /**< Current LTP threshold jitter */
    float ltd_threshold_noise;       /**< Current LTD threshold jitter */
    float ltp_cost_noise;            /**< Current LTP cost variation */
    float ltd_cost_noise;            /**< Current LTD cost variation */
    float delivery_noise;            /**< Current delivery fluctuation */

    /* Effective noise parameters after adaptation */
    float effective_amplitude;       /**< Current noise amplitude */
    float effective_alpha;           /**< Current spectral exponent */
} metabolic_pink_noise_state_t;

/**
 * @brief Metabolic pink noise bridge
 *
 * Main bridge structure connecting metabolic plasticity and pink noise
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    metabolic_plasticity_t* metabolic;       /**< Metabolic plasticity system */
    pink_noise_generator_t noise_generator;  /**< Pink noise generator */

    metabolic_pink_noise_config_t config;    /**< Configuration */
    metabolic_pink_noise_state_t state;      /**< Current state */

    /* Statistics */
    uint64_t update_count;                   /**< Total updates */
    float avg_recovery_noise;                /**< Average recovery noise */
    float avg_threshold_jitter;              /**< Average threshold jitter */
    float avg_cost_variation;                /**< Average cost variation */

    void* bio_ctx;                           /**< Bio-async context */} metabolic_pink_noise_bridge_t;

/**
 * @brief Bridge statistics
 */
typedef struct {
    uint64_t total_updates;                  /**< Total update cycles */
    float avg_recovery_noise;                /**< Average recovery modulation */
    float avg_threshold_jitter;              /**< Average threshold jitter */
    float avg_cost_variation;                /**< Average cost variation */
    float current_amplitude;                 /**< Current noise amplitude */
    float current_alpha;                     /**< Current spectral exponent */
} metabolic_pink_noise_stats_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default configuration
 *
 * WHAT: Provide sensible defaults for metabolic pink noise integration
 * WHY:  Easy initialization with biologically realistic parameters
 * HOW:  Return struct with evidence-based defaults
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int metabolic_pink_noise_default_config(metabolic_pink_noise_config_t* config);

/**
 * @brief Create metabolic pink noise bridge
 *
 * WHAT: Initialize bridge for metabolic-noise integration
 * WHY:  Enable stochastic energy dynamics
 * HOW:  Allocate structure, create noise generator, initialize state
 *
 * @param config Bridge configuration (NULL for defaults)
 * @param metabolic Metabolic plasticity system (optional, can connect later)
 * @return Bridge handle or NULL on failure
 */
metabolic_pink_noise_bridge_t* metabolic_pink_noise_create(
    const metabolic_pink_noise_config_t* config,
    metabolic_plasticity_t* metabolic
);

/**
 * @brief Destroy metabolic pink noise bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Destroy noise generator, free structure and mutex
 *
 * @param bridge Bridge to destroy (NULL-safe)
 */
void metabolic_pink_noise_destroy(metabolic_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect to metabolic plasticity system
 *
 * WHAT: Establish connection to metabolic system
 * WHY:  Enable bidirectional modulation
 * HOW:  Store pointer, validate compatibility
 *
 * @param bridge Pink noise bridge
 * @param metabolic Metabolic plasticity system
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_connect_metabolic(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_plasticity_t* metabolic
);

/**
 * @brief Disconnect from metabolic system
 *
 * WHAT: Remove connection to metabolic system
 * WHY:  Clean shutdown or reconfiguration
 * HOW:  Clear pointer, reset state
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_disconnect(metabolic_pink_noise_bridge_t* bridge);

/**
 * @brief Check if connected to metabolic system
 *
 * WHAT: Query connection status
 * WHY:  Validate before operations
 * HOW:  Check if metabolic pointer is non-NULL
 *
 * @param bridge Pink noise bridge
 * @return true if connected
 */
bool metabolic_pink_noise_is_connected(const metabolic_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Update API
 * ============================================================================ */

/**
 * @brief Update noise modulation
 *
 * WHAT: Generate new noise samples and update modulation state
 * WHY:  Maintain continuous stochastic modulation
 * HOW:  Generate samples, apply strength scaling, update state
 *
 * @param bridge Pink noise bridge
 * @param delta_ms Time since last update (milliseconds)
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_update(
    metabolic_pink_noise_bridge_t* bridge,
    uint64_t delta_ms
);

/**
 * @brief Apply noise to recovery rate
 *
 * WHAT: Modulate ATP recovery rate with pink noise
 * WHY:  Model stochastic mitochondrial dynamics
 * HOW:  base_rate *= (1 + noise * strength)
 *
 * @param bridge Pink noise bridge
 * @param base_rate Base recovery rate
 * @return Modulated recovery rate
 */
float metabolic_pink_noise_apply_recovery(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_rate
);

/**
 * @brief Apply noise to threshold
 *
 * WHAT: Add jitter to energy threshold
 * WHY:  Model stochastic threshold variability
 * HOW:  threshold += noise * jitter_strength
 *
 * @param bridge Pink noise bridge
 * @param base_threshold Base threshold value
 * @param target Threshold target (LTP or LTD)
 * @return Modulated threshold
 */
float metabolic_pink_noise_apply_threshold(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_threshold,
    metabolic_noise_target_t target
);

/**
 * @brief Apply noise to energy cost
 *
 * WHAT: Modulate energy cost with pink noise
 * WHY:  Model stochastic energy demand
 * HOW:  cost *= (1 + noise * variability)
 *
 * @param bridge Pink noise bridge
 * @param base_cost Base energy cost
 * @param target Cost target (LTP or LTD)
 * @return Modulated cost
 */
float metabolic_pink_noise_apply_cost(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_cost,
    metabolic_noise_target_t target
);

/**
 * @brief Apply noise to glucose delivery
 *
 * WHAT: Add stochastic fluctuations to glucose delivery
 * WHY:  Model variable blood flow and astrocyte shuttle
 * HOW:  delivery += noise * delivery_strength
 *
 * @param bridge Pink noise bridge
 * @param base_delivery Base delivery rate
 * @return Modulated delivery rate
 */
float metabolic_pink_noise_apply_delivery(
    const metabolic_pink_noise_bridge_t* bridge,
    float base_delivery
);

/* ============================================================================
 * Query API
 * ============================================================================ */

/**
 * @brief Get current noise state
 *
 * WHAT: Query current modulation values
 * WHY:  Monitor noise effects
 * HOW:  Copy state struct to output
 *
 * @param bridge Pink noise bridge
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_get_state(
    const metabolic_pink_noise_bridge_t* bridge,
    metabolic_pink_noise_state_t* state
);

/**
 * @brief Get effective amplitude
 *
 * WHAT: Query current noise amplitude (after energy adaptation)
 * WHY:  Monitor amplitude changes
 * HOW:  Return effective_amplitude from state
 *
 * @param bridge Pink noise bridge
 * @return Effective amplitude
 */
float metabolic_pink_noise_get_amplitude(const metabolic_pink_noise_bridge_t* bridge);

/**
 * @brief Get effective alpha
 *
 * WHAT: Query current spectral exponent (after energy adaptation)
 * WHY:  Monitor spectrum changes
 * HOW:  Return effective_alpha from state
 *
 * @param bridge Pink noise bridge
 * @return Effective spectral exponent
 */
float metabolic_pink_noise_get_alpha(const metabolic_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Statistics API
 * ============================================================================ */

/**
 * @brief Get bridge statistics
 *
 * WHAT: Query accumulated statistics
 * WHY:  Monitor modulation patterns
 * HOW:  Copy stats to output
 *
 * @param bridge Pink noise bridge
 * @param stats Output statistics structure
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_get_stats(
    const metabolic_pink_noise_bridge_t* bridge,
    metabolic_pink_noise_stats_t* stats
);

/**
 * @brief Reset statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh monitoring period
 * HOW:  Zero all stat counters
 *
 * @param bridge Pink noise bridge
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_reset_stats(metabolic_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Control API
 * ============================================================================ */

/**
 * @brief Enable specific noise target
 *
 * WHAT: Enable modulation for specific metabolic parameter
 * WHY:  Selective control of noise application
 * HOW:  Set feature flag in config
 *
 * @param bridge Pink noise bridge
 * @param target Noise target to enable
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_enable_target(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target
);

/**
 * @brief Disable specific noise target
 *
 * WHAT: Disable modulation for specific metabolic parameter
 * WHY:  Selective control of noise application
 * HOW:  Clear feature flag in config
 *
 * @param bridge Pink noise bridge
 * @param target Noise target to disable
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_disable_target(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target
);

/**
 * @brief Set noise strength for target
 *
 * WHAT: Adjust modulation strength for specific parameter
 * WHY:  Fine-tune biological realism
 * HOW:  Update strength value in config
 *
 * @param bridge Pink noise bridge
 * @param target Noise target
 * @param strength New strength value [0-1]
 * @return 0 on success, negative on error
 */
int metabolic_pink_noise_set_strength(
    metabolic_pink_noise_bridge_t* bridge,
    metabolic_noise_target_t target,
    float strength
);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * @brief Get noise target name
 *
 * WHAT: Convert noise target enum to string
 * WHY:  Logging and debugging
 * HOW:  Return static string for target
 *
 * @param target Noise target
 * @return Target name string
 */
const char* metabolic_pink_noise_target_name(metabolic_noise_target_t target);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_PINK_NOISE_BRIDGE_H */
