/**
 * @file nimcp_stp_pink_noise_bridge.h
 * @brief Pink Noise - Short-Term Plasticity Integration Bridge
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Bidirectional integration between pink noise and short-term plasticity (STP)
 * WHY:  1/f noise modulates STP dynamics for biological realism; STP provides stochastic vesicle release.
 *       Essential for naturalistic synaptic transmission matching in vivo variability.
 * HOW:  Pink noise modulates release probability U and time constants τ_D/τ_F; noise amplitude
 *       scales with STP state (u,x) to model activity-dependent stochasticity.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * PINK NOISE → STP PATHWAYS:
 * ----------------------------
 * 1. Stochastic Vesicle Release:
 *    - Release probability U fluctuates with pink noise (1/f spectrum)
 *    - Reflects biological variability in calcium dynamics
 *    - U_effective(t) = U_base × (1 + α·pink_noise(t))
 *    - Reference: Faisal et al. (2008) "Noise in the nervous system" Nat Rev Neurosci
 *
 * 2. Time Constant Variability:
 *    - Recovery (τ_D) and facilitation (τ_F) vary with 1/f statistics
 *    - Models fluctuations in vesicle pool replenishment
 *    - τ_effective = τ_base × (1 + β·pink_noise(t))
 *    - Reference: Destexhe et al. (2003) "Fluctuating synaptic conductances" J Comput Neurosci
 *
 * 3. Multi-Timescale Modulation:
 *    - Pink noise provides natural slow + fast components
 *    - Slow fluctuations: metabolic state, neuromodulatory tone (seconds-minutes)
 *    - Fast fluctuations: vesicle dynamics, calcium buffering (milliseconds)
 *    - Reference: Bédard et al. (2006) "Does the 1/f frequency scaling of brain signals reflect
 *      self-organized critical states?" Phys Rev Lett
 *
 * STP → PINK NOISE PATHWAYS:
 * ---------------------------
 * 1. Activity-Dependent Noise Amplitude:
 *    - Noise amplitude scales with STP state (u×x)
 *    - High activity → stronger modulation (more stochasticity)
 *    - Low activity → reduced modulation (more reliable)
 *    - Implements activity-dependent gain control
 *
 * 2. State-Dependent Noise Frequency:
 *    - Depleted synapses (low x) → shift to lower frequencies
 *    - Facilitated synapses (high u) → shift to higher frequencies
 *    - Adaptive frequency modulation based on resource state
 *
 * INTEGRATION MECHANISMS:
 * ------------------------
 * - Additive modulation: U_eff = U_base + amplitude × pink_noise(t)
 * - Multiplicative modulation: τ_eff = τ_base × (1 + strength × pink_noise(t))
 * - State-scaling: noise_amplitude(t) = base_amplitude × sqrt(u(t) × x(t))
 * - Biological constraint: U_eff ∈ [U_min, U_max], τ_eff ∈ [τ_min, τ_max]
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

#ifndef NIMCP_STP_PINK_NOISE_BRIDGE_H
#define NIMCP_STP_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include <stddef.h>

/* Module integrations */
#include "plasticity/stp/nimcp_stp.h"
#include "plasticity/noise/nimcp_pink_noise.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Release probability modulation */
#define STP_PINK_U_MIN_FACTOR           0.5f    /**< Min U scaling (50% of base) */
#define STP_PINK_U_MAX_FACTOR           1.5f    /**< Max U scaling (150% of base) */
#define STP_PINK_U_NOISE_AMPLITUDE      0.1f    /**< Default U noise amplitude */

/* Time constant modulation */
#define STP_PINK_TAU_MIN_FACTOR         0.7f    /**< Min τ scaling (70% of base) */
#define STP_PINK_TAU_MAX_FACTOR         1.3f    /**< Max τ scaling (130% of base) */
#define STP_PINK_TAU_NOISE_AMPLITUDE    0.15f   /**< Default τ noise amplitude */

/* Activity-dependent scaling */
#define STP_PINK_STATE_SCALING_MIN      0.5f    /**< Min state-based scaling */
#define STP_PINK_STATE_SCALING_MAX      2.0f    /**< Max state-based scaling */
#define STP_PINK_STATE_SENSITIVITY      1.0f    /**< State → noise amplitude scaling */

/* Noise configuration defaults */
#define STP_PINK_ALPHA_DEFAULT          1.0f    /**< Default spectral exponent (true pink) */
#define STP_PINK_SAMPLE_RATE_DEFAULT    1000.0f /**< Default sample rate (1ms timestep) */

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct stp_pink_noise_bridge stp_pink_noise_bridge_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Configuration for STP-Pink Noise bridge
 */
typedef struct {
    /* Release probability modulation */
    float u_noise_amplitude;            /**< Amplitude of U modulation */
    float u_min_factor;                 /**< Min U scaling factor */
    float u_max_factor;                 /**< Max U scaling factor */

    /* Time constant modulation */
    float tau_d_noise_amplitude;        /**< Amplitude of τ_D modulation */
    float tau_f_noise_amplitude;        /**< Amplitude of τ_F modulation */
    float tau_min_factor;               /**< Min τ scaling factor */
    float tau_max_factor;               /**< Max τ scaling factor */

    /* Activity-dependent modulation */
    bool enable_state_scaling;          /**< Enable STP state-based noise scaling */
    float state_sensitivity;            /**< State → noise amplitude scaling */
    float state_scaling_min;            /**< Min state scaling factor */
    float state_scaling_max;            /**< Max state scaling factor */

    /* Noise generator configuration */
    pink_noise_config_t u_noise_config;     /**< Config for U modulation noise */
    pink_noise_config_t tau_noise_config;   /**< Config for τ modulation noise */

    /* Feature enables */
    bool enable_u_modulation;           /**< Enable release probability modulation */
    bool enable_tau_d_modulation;       /**< Enable depression time constant modulation */
    bool enable_tau_f_modulation;       /**< Enable facilitation time constant modulation */
} stp_pink_noise_config_t;

/**
 * @brief Pink noise effects on STP
 */
typedef struct {
    /* Current noise values */
    float u_noise_value;                /**< Current U noise sample */
    float tau_d_noise_value;            /**< Current τ_D noise sample */
    float tau_f_noise_value;            /**< Current τ_F noise sample */

    /* State-dependent scaling */
    float current_state_scaling;        /**< Current activity-based scaling (from u×x) */

    /* Effective modulated values */
    float effective_u_factor;           /**< Total U modulation factor */
    float effective_tau_d_factor;       /**< Total τ_D modulation factor */
    float effective_tau_f_factor;       /**< Total τ_F modulation factor */
} stp_pink_noise_effects_t;

/**
 * @brief STP effects on pink noise (feedback)
 */
typedef struct {
    /* Current STP state */
    float current_u;                    /**< Current facilitation level */
    float current_x;                    /**< Current resource level */
    float current_transmission;         /**< Current u×x transmission */

    /* Derived noise control */
    float activity_amplitude_scaling;   /**< u×x-derived noise amplitude */
    float depletion_frequency_shift;    /**< x-derived frequency adjustment */
    float facilitation_frequency_shift; /**< u-derived frequency adjustment */
} stp_pink_noise_feedback_t;

/**
 * @brief Current state of STP-Pink Noise interaction
 */
typedef struct {
    /* Current STP parameters */
    float base_u;                       /**< Baseline release probability */
    float base_tau_d;                   /**< Baseline depression time constant */
    float base_tau_f;                   /**< Baseline facilitation time constant */

    /* Current STP state */
    float current_u;                    /**< Current facilitation */
    float current_x;                    /**< Current resources */

    /* Applied modulations */
    float u_modulation;                 /**< Current U modulation factor */
    float tau_d_modulation;             /**< Current τ_D modulation factor */
    float tau_f_modulation;             /**< Current τ_F modulation factor */

    /* Activity state */
    float state_scaling;                /**< Current activity-based scaling */

    /* Statistics */
    uint32_t modulation_events;         /**< Total modulation events */
    uint64_t last_update_time;          /**< Last update timestamp (ms) */
} stp_pink_noise_state_t;

/**
 * @brief Statistics for STP-Pink Noise bridge
 */
typedef struct {
    /* Update counts */
    uint64_t total_updates;             /**< Total bridge updates */
    uint64_t u_modulated_events;        /**< U-modulated events */
    uint64_t tau_modulated_events;      /**< τ-modulated events */

    /* Noise statistics */
    float avg_u_noise;                  /**< Average U noise value */
    float avg_tau_d_noise;              /**< Average τ_D noise value */
    float avg_tau_f_noise;              /**< Average τ_F noise value */

    /* Modulation statistics */
    float avg_u_modulation;             /**< Average U modulation factor */
    float avg_tau_d_modulation;         /**< Average τ_D modulation factor */
    float avg_tau_f_modulation;         /**< Average τ_F modulation factor */

    /* STP state statistics */
    float avg_facilitation;             /**< Average facilitation (u) */
    float avg_resources;                /**< Average resources (x) */
    float avg_transmission;             /**< Average u×x */
    float avg_state_scaling;            /**< Average state-based noise scaling */
} stp_pink_noise_stats_t;

/**
 * @brief STP-Pink Noise bridge state
 */
struct stp_pink_noise_bridge {
    bridge_base_t base;               /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    stp_pink_noise_config_t config;

    /* Connected systems */
    stp_state_t* stp_state;             /**< STP state */
    pink_noise_generator_t u_noise_gen; /**< U noise generator */
    pink_noise_generator_t tau_noise_gen; /**< τ noise generator */

    /* Current effects */
    stp_pink_noise_effects_t noise_effects;
    stp_pink_noise_feedback_t stp_feedback;
    stp_pink_noise_state_t state;

    /* Statistics */
    stp_pink_noise_stats_t stats;

};

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default STP-Pink Noise configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible defaults
 * HOW:  Set standard modulation amplitudes and enable all features
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int stp_pink_noise_default_config(stp_pink_noise_config_t* config);

/**
 * @brief Create STP-Pink Noise bridge
 *
 * WHAT: Initialize STP-pink noise integration bridge
 * WHY:  Enable bidirectional STP-noise interaction
 * HOW:  Allocate bridge, create noise generators, initialize state
 *
 * @param config Configuration (NULL for defaults)
 * @return New bridge or NULL on failure
 */
stp_pink_noise_bridge_t* stp_pink_noise_create(const stp_pink_noise_config_t* config);

/**
 * @brief Destroy STP-Pink Noise bridge
 *
 * WHAT: Clean up bridge resources
 * WHY:  Proper resource deallocation
 * HOW:  Destroy noise generators, disconnect systems, free memory
 *
 * @param bridge Bridge to destroy (NULL safe)
 */
void stp_pink_noise_destroy(stp_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Connection API
 * ============================================================================ */

/**
 * @brief Connect STP state
 *
 * WHAT: Link bridge to STP state
 * WHY:  Enable STP parameter modulation
 * HOW:  Store STP state pointer
 *
 * @param bridge STP-Pink Noise bridge
 * @param stp_state STP state
 * @return 0 on success, -1 on error
 */
int stp_pink_noise_connect_stp(
    stp_pink_noise_bridge_t* bridge,
    stp_state_t* stp_state
);

/**
 * @brief Disconnect STP state
 *
 * WHAT: Unlink STP state
 * WHY:  Safe shutdown
 * HOW:  Clear STP state pointer
 *
 * @param bridge STP-Pink Noise bridge
 * @return 0 on success
 */
int stp_pink_noise_disconnect(stp_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Pink Noise → STP Direction
 * ============================================================================ */

/**
 * @brief Apply pink noise modulation to release probability
 *
 * WHAT: Modulate baseline release probability U by pink noise
 * WHY:  Stochastic vesicle release matches biological variability
 * HOW:  U_effective = U_base × (1 + amplitude × pink_noise(t))
 *
 * @param bridge STP-Pink Noise bridge
 * @return Release probability modulation factor (clamped to valid range)
 */
float stp_pink_noise_modulate_u(stp_pink_noise_bridge_t* bridge);

/**
 * @brief Apply pink noise modulation to depression time constant
 *
 * WHAT: Modulate depression recovery τ_D by pink noise
 * WHY:  Variable vesicle pool replenishment rates
 * HOW:  τ_D_effective = τ_D_base × (1 + amplitude × pink_noise(t))
 *
 * @param bridge STP-Pink Noise bridge
 * @return τ_D modulation factor (clamped to valid range)
 */
float stp_pink_noise_modulate_tau_d(stp_pink_noise_bridge_t* bridge);

/**
 * @brief Apply pink noise modulation to facilitation time constant
 *
 * WHAT: Modulate facilitation decay τ_F by pink noise
 * WHY:  Variable calcium buffering dynamics
 * HOW:  τ_F_effective = τ_F_base × (1 + amplitude × pink_noise(t))
 *
 * @param bridge STP-Pink Noise bridge
 * @return τ_F modulation factor (clamped to valid range)
 */
float stp_pink_noise_modulate_tau_f(stp_pink_noise_bridge_t* bridge);

/**
 * @brief Get effective release probability with noise
 *
 * WHAT: Compute noise-modulated release probability
 * WHY:  Single function to get stochastic U for STP updates
 * HOW:  Apply noise modulation and state scaling to base U
 *
 * @param bridge STP-Pink Noise bridge
 * @param base_u Base release probability
 * @return Effective release probability
 */
float stp_pink_noise_get_effective_u(
    const stp_pink_noise_bridge_t* bridge,
    float base_u
);

/**
 * @brief Get effective time constants with noise
 *
 * WHAT: Compute noise-modulated time constants
 * WHY:  Single function to get stochastic τ_D and τ_F
 * HOW:  Apply noise modulation and state scaling
 *
 * @param bridge STP-Pink Noise bridge
 * @param base_tau_d Base depression time constant
 * @param base_tau_f Base facilitation time constant
 * @param tau_d_out Output effective τ_D
 * @param tau_f_out Output effective τ_F
 * @return 0 on success
 */
int stp_pink_noise_get_effective_tau(
    const stp_pink_noise_bridge_t* bridge,
    float base_tau_d,
    float base_tau_f,
    float* tau_d_out,
    float* tau_f_out
);

/* ============================================================================
 * STP → Pink Noise Direction
 * ============================================================================ */

/**
 * @brief Update noise amplitude based on STP state
 *
 * WHAT: Scale noise amplitude by current STP activity (u×x)
 * WHY:  High activity increases stochasticity
 * HOW:  amplitude_eff = amplitude_base × sqrt(u × x)
 *
 * @param bridge STP-Pink Noise bridge
 * @param u Current facilitation level
 * @param x Current resource level
 * @return Activity-scaled noise amplitude factor
 */
float stp_pink_noise_scale_by_activity(
    stp_pink_noise_bridge_t* bridge,
    float u,
    float x
);

/**
 * @brief Report STP state to bridge
 *
 * WHAT: Update bridge with current STP state for feedback
 * WHY:  Enable activity-dependent noise modulation
 * HOW:  Store u, x and compute derived metrics
 *
 * @param bridge STP-Pink Noise bridge
 * @param u Current facilitation level
 * @param x Current resource level
 * @return 0 on success
 */
int stp_pink_noise_report_stp_state(
    stp_pink_noise_bridge_t* bridge,
    float u,
    float x
);

/* ============================================================================
 * Update Cycle
 * ============================================================================ */

/**
 * @brief Update STP-Pink Noise bridge state
 *
 * WHAT: Main update loop for bidirectional integration
 * WHY:  Generate new noise samples and apply modulation
 * HOW:  Sample noise generators, compute effects, update statistics
 *
 * @param bridge STP-Pink Noise bridge
 * @return 0 on success
 */
int stp_pink_noise_update(stp_pink_noise_bridge_t* bridge);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current bridge state
 *
 * @param bridge STP-Pink Noise bridge
 * @param state Output state
 * @return 0 on success
 */
int stp_pink_noise_get_state(
    const stp_pink_noise_bridge_t* bridge,
    stp_pink_noise_state_t* state
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge STP-Pink Noise bridge
 * @param stats Output statistics
 * @return 0 on success
 */
int stp_pink_noise_get_stats(
    const stp_pink_noise_bridge_t* bridge,
    stp_pink_noise_stats_t* stats
);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clear accumulated statistics
 * WHY:  Start fresh measurement period
 * HOW:  Zero all counters and accumulators
 *
 * @param bridge STP-Pink Noise bridge
 * @return 0 on success
 */
int stp_pink_noise_reset_stats(stp_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Feature Control
 * ============================================================================ */

/**
 * @brief Enable/disable pink noise modulation
 *
 * WHAT: Toggle noise modulation on/off
 * WHY:  Allow dynamic control for testing/comparison
 * HOW:  Set enable flags in configuration
 *
 * @param bridge STP-Pink Noise bridge
 * @param enable true to enable, false to disable
 * @return 0 on success
 */
int stp_pink_noise_enable(stp_pink_noise_bridge_t* bridge, bool enable);

/**
 * @brief Check if pink noise modulation is enabled
 *
 * @param bridge STP-Pink Noise bridge
 * @return true if any modulation is enabled
 */
bool stp_pink_noise_is_enabled(const stp_pink_noise_bridge_t* bridge);

/* ============================================================================
 * Bio-Async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Enable bio-async messaging for STP-noise coordination
 * WHY:  Distributed synaptic noise signaling
 * HOW:  Register module, set up handlers
 *
 * @param bridge STP-Pink Noise bridge
 * @return 0 on success
 */
int stp_pink_noise_connect_bio_async(stp_pink_noise_bridge_t* bridge);

/**
 * @brief Disconnect from bio-async router
 *
 * @param bridge STP-Pink Noise bridge
 * @return 0 on success
 */
int stp_pink_noise_disconnect_bio_async(stp_pink_noise_bridge_t* bridge);

/**
 * @brief Check if bio-async is connected
 *
 * @param bridge STP-Pink Noise bridge
 * @return true if bio-async enabled
 */
bool stp_pink_noise_is_bio_async_connected(
    const stp_pink_noise_bridge_t* bridge
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_STP_PINK_NOISE_BRIDGE_H */
