/**
 * @file nimcp_fep_neuromod.h
 * @brief Neuromodulator Precision Weighting for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Neuromodulator-based precision modulation for FEP prediction errors
 * WHY:  Biological realism - neuromodulators control gain on prediction errors,
 *       implementing attention-as-precision-optimization in active inference
 * HOW:  Model 4 neuromodulatory systems (ACh, NE, DA, 5-HT) that dynamically
 *       weight prediction errors based on uncertainty, salience, reward, and time
 *
 * THEORETICAL FOUNDATION:
 * ==================================================================================
 *
 * NEUROMODULATION OF PRECISION IN FEP:
 * ------------------------------------
 * In the Free Energy Principle, prediction errors are precision-weighted:
 *
 *   ε_weighted = Π * ε
 *
 * Where Π (precision) is the inverse variance of sensory noise. In the brain,
 * neuromodulators dynamically adjust these precision weights, implementing
 * attention as precision optimization.
 *
 * ACETYLCHOLINE (ACh) - EXPECTED VS UNEXPECTED UNCERTAINTY:
 * ---------------------------------------------------------
 * ACh signals the reliability of predictions (expected uncertainty):
 *   - High ACh → High precision → Predictions are reliable
 *   - Low ACh → Low precision → Predictions are unreliable
 *
 * Biological source: Basal forebrain (nucleus basalis) → cortex
 * FEP role: Modulates precision based on confidence in generative model
 *
 * Computational model:
 *   ACh(t+1) = ACh(t) * (1 - decay) + gain * (1 - epistemic_uncertainty)
 *   Π_ach = baseline * (1 + ACh * ach_gain)
 *
 * NOREPINEPHRINE (NE) - UNEXPECTED UNCERTAINTY (SALIENCE):
 * --------------------------------------------------------
 * NE signals unexpected, surprising events (unexpected uncertainty):
 *   - High NE → High precision → Surprising inputs are salient
 *   - Spike on surprise → Amplify prediction error signal
 *
 * Biological source: Locus coeruleus → widespread cortex
 * FEP role: Amplifies precision for high-surprise events
 *
 * Computational model:
 *   NE(t+1) = NE(t) * (1 - decay) + gain * surprise
 *   Π_ne = 1 + NE * ne_gain
 *
 * DOPAMINE (DA) - REWARD PREDICTION ERROR:
 * ----------------------------------------
 * DA signals reward prediction errors, modulating exploration/exploitation:
 *   - Positive DA → Unexpected reward → Explore
 *   - Negative DA → Expected reward omitted → Exploit
 *
 * Biological source: VTA/SNc → striatum, prefrontal cortex
 * FEP role: Modulates expected free energy for action selection
 *
 * Computational model:
 *   DA(t+1) = DA(t) * (1 - decay) + gain * (actual_value - expected_value)
 *   exploration_bonus = DA * exploration_rate
 *
 * SEROTONIN (5-HT) - TEMPORAL DISCOUNTING:
 * ----------------------------------------
 * 5-HT modulates patience in planning (temporal horizon):
 *   - High 5-HT → Patient → Long planning horizon
 *   - Low 5-HT → Impulsive → Short planning horizon
 *
 * Biological source: Raphe nuclei → frontal cortex
 * FEP role: Modulates temporal discounting in expected free energy
 *
 * Computational model:
 *   5-HT(t+1) = 5-HT(t) * (1 - decay) + baseline
 *   temporal_horizon = base_horizon * (1 + 5-HT * horizon_gain)
 *
 * INTEGRATED PRECISION COMPUTATION:
 * ---------------------------------
 * Final precision combines all neuromodulators:
 *
 *   Π_total = Π_base * Π_ach * Π_ne * (1 + exploration_bonus)
 *
 * Where:
 *   Π_base = Base precision from sensory model
 *   Π_ach = ACh-modulated precision (expected uncertainty)
 *   Π_ne = NE-modulated precision (unexpected uncertainty)
 *   exploration_bonus = DA-modulated exploration drive
 *
 * REFERENCES:
 * - Yu & Dayan (2005) "Uncertainty, neuromodulation, and attention"
 * - Friston et al. (2012) "Dopamine, affordance and active inference"
 * - Marshall et al. (2016) "Pharmacological fingerprints of contextual uncertainty"
 * - Parr & Friston (2017) "Uncertainty, epistemics and active inference"
 * - Schwartenbeck et al. (2015) "The dopaminergic midbrain encodes EFE"
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                   NEUROMODULATOR PRECISION SYSTEM                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  NEUROMODULATOR DYNAMICS                            │  ║
 * ║   │                                                                     │  ║
 * ║   │   ACh(t+1) = ACh(t) * decay + (1 - uncertainty)                    │  ║
 * ║   │   NE(t+1) = NE(t) * decay + surprise                               │  ║
 * ║   │   DA(t+1) = DA(t) * decay + reward_prediction_error                │  ║
 * ║   │   5-HT(t+1) = 5-HT(t) * decay + baseline                           │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  PRECISION MODULATION                               │  ║
 * ║   │                                                                     │  ║
 * ║   │   Π_ach = 1 + ACh * ach_gain                                       │  ║
 * ║   │   Π_ne = 1 + NE * ne_gain                                          │  ║
 * ║   │   Exploration = DA * exploration_rate                               │  ║
 * ║   │   Horizon = base * (1 + 5-HT * horizon_gain)                       │  ║
 * ║   │                                                                     │  ║
 * ║   │   Π_total = Π_base * Π_ach * Π_ne                                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                  FEP INTEGRATION                                    │  ║
 * ║   │                                                                     │  ║
 * ║   │   FEP Observation → Surprise → NE release                          │  ║
 * ║   │   FEP Uncertainty → ACh modulation                                 │  ║
 * ║   │   FEP Reward → DA release                                          │  ║
 * ║   │   Precision → FEP belief updates                                   │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_NEUROMOD_H
#define NIMCP_FEP_NEUROMOD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "cognitive/free_energy/nimcp_free_energy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Default baselines (normalized 0-1) */
#define FEP_NEUROMOD_DEFAULT_ACH_BASELINE       0.5f
#define FEP_NEUROMOD_DEFAULT_NE_BASELINE        0.2f
#define FEP_NEUROMOD_DEFAULT_DA_BASELINE        0.5f
#define FEP_NEUROMOD_DEFAULT_5HT_BASELINE       0.5f

/* Default decay rates (per second) */
#define FEP_NEUROMOD_DEFAULT_ACH_DECAY          0.5f   /**< Fast decay (2s half-life) */
#define FEP_NEUROMOD_DEFAULT_NE_DECAY           0.3f   /**< Medium decay (3s half-life) */
#define FEP_NEUROMOD_DEFAULT_DA_DECAY           0.35f  /**< Medium decay (2.5s half-life) */
#define FEP_NEUROMOD_DEFAULT_5HT_DECAY          0.07f  /**< Slow decay (10s half-life) */

/* Default gains */
#define FEP_NEUROMOD_DEFAULT_PRECISION_GAIN_ACH 0.8f   /**< ACh precision impact */
#define FEP_NEUROMOD_DEFAULT_PRECISION_GAIN_NE  1.2f   /**< NE precision amplification */
#define FEP_NEUROMOD_DEFAULT_EXPLORATION_RATE   0.3f   /**< DA exploration rate */
#define FEP_NEUROMOD_DEFAULT_TEMPORAL_DISCOUNT  0.5f   /**< 5-HT horizon modulation */

/* Bounds */
#define FEP_NEUROMOD_MIN_LEVEL                  0.0f
#define FEP_NEUROMOD_MAX_LEVEL                  1.0f
#define FEP_NEUROMOD_MIN_PRECISION              0.1f
#define FEP_NEUROMOD_MAX_PRECISION              10.0f

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Neuromodulator types for FEP precision weighting
 *
 * BIOLOGICAL MAPPING:
 * - ACh: Basal forebrain → Expected uncertainty
 * - NE: Locus coeruleus → Unexpected uncertainty (surprise)
 * - DA: VTA/SNc → Reward prediction error
 * - 5-HT: Raphe nuclei → Temporal discounting
 */
typedef enum {
    FEP_NEUROMOD_ACH = 0,    /**< Acetylcholine - expected uncertainty */
    FEP_NEUROMOD_NE,         /**< Norepinephrine - unexpected uncertainty (salience) */
    FEP_NEUROMOD_DA,         /**< Dopamine - value/reward prediction error */
    FEP_NEUROMOD_5HT,        /**< Serotonin - patience/temporal discounting */
    FEP_NEUROMOD_COUNT       /**< Total count */
} fep_neuromod_type_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Neuromodulator system configuration
 */
typedef struct {
    /* Baseline levels (resting state) */
    float ach_baseline;         /**< ACh baseline [0,1] */
    float ne_baseline;          /**< NE baseline [0,1] */
    float da_baseline;          /**< DA baseline [0,1] */
    float serotonin_baseline;   /**< 5-HT baseline [0,1] */

    /* Decay rates (per second) */
    float ach_decay_rate;       /**< ACh decay rate */
    float ne_decay_rate;        /**< NE decay rate */
    float da_decay_rate;        /**< DA decay rate */
    float serotonin_decay_rate; /**< 5-HT decay rate */

    /* Precision gains */
    float precision_gain_ach;   /**< ACh → precision multiplier */
    float precision_gain_ne;    /**< NE → precision amplification */
    float exploration_rate_da;  /**< DA → exploration bonus */
    float temporal_discount_5ht;/**< 5-HT → temporal horizon modulation */

    /* Adaptive gain control */
    bool enable_adaptive_gain;  /**< Auto-tune gains based on performance */
} fep_neuromod_config_t;

/**
 * @brief Current neuromodulator state
 */
typedef struct {
    float levels[FEP_NEUROMOD_COUNT];  /**< Current levels [0,1] */
    float precision_multiplier;         /**< Combined precision effect */
    float exploration_bonus;            /**< DA-driven exploration */
    float temporal_horizon;             /**< 5-HT-driven planning horizon */
} fep_neuromod_state_t;

/**
 * @brief Complete neuromodulator system (opaque)
 */
typedef struct {
    /* Configuration */
    fep_neuromod_config_t config;

    /* Current state */
    fep_neuromod_state_t state;

    /* FEP integration */
    fep_system_t* fep_system;      /**< Connected FEP system */

    /* Bio-async integration */
    bio_module_context_t bio_ctx;  /**< Bio-async module context */
    bool bio_async_enabled;        /**< Bio-async connection state */

    /* Timing */
    uint64_t last_update_ms;       /**< Last update timestamp */

    /* Statistics */
    uint64_t total_updates;
    uint64_t ach_releases;
    uint64_t ne_releases;
    uint64_t da_releases;
    uint64_t serotonin_releases;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} fep_neuromod_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default neuromodulator configuration
 *
 * WHAT: Provide sensible default configuration
 * WHY:  Easy initialization with biologically-plausible parameters
 * HOW:  Set baseline levels, decay rates, and gains from literature
 *
 * @param config Output configuration
 * @return 0 on success, -1 on error
 */
int fep_neuromod_default_config(fep_neuromod_config_t* config);

/**
 * @brief Create neuromodulator system
 *
 * WHAT: Initialize neuromodulator precision weighting system
 * WHY:  Enable biologically-realistic attention and precision control
 * HOW:  Allocate system, initialize levels to baseline, create mutex
 *
 * @param config Configuration (NULL for defaults)
 * @return New neuromodulator system or NULL on failure
 */
fep_neuromod_system_t* fep_neuromod_create(const fep_neuromod_config_t* config);

/**
 * @brief Destroy neuromodulator system
 *
 * WHAT: Clean up neuromodulator system resources
 * WHY:  Prevent memory leaks
 * HOW:  Disconnect bio-async, destroy mutex, free memory
 *
 * @param sys Neuromodulator system (NULL safe)
 */
void fep_neuromod_destroy(fep_neuromod_system_t* sys);

/* ============================================================================
 * Neuromodulator Dynamics API
 * ============================================================================ */

/**
 * @brief Update neuromodulator dynamics
 *
 * WHAT: Apply exponential decay to all neuromodulator levels
 * WHY:  Neuromodulators naturally decay over time
 * HOW:  level(t+1) = level(t) * exp(-decay * dt) + baseline
 *
 * @param sys Neuromodulator system
 * @param delta_ms Time step in milliseconds
 * @return 0 on success, negative on error
 */
int fep_neuromod_update(fep_neuromod_system_t* sys, uint64_t delta_ms);

/**
 * @brief Release neuromodulator (increase level)
 *
 * WHAT: Increase neuromodulator level by amount
 * WHY:  Events trigger neuromodulator release
 * HOW:  level += amount, clamp to [0,1]
 *
 * @param sys Neuromodulator system
 * @param type Neuromodulator type
 * @param amount Amount to release [0,1]
 * @return 0 on success, negative on error
 */
int fep_neuromod_release(
    fep_neuromod_system_t* sys,
    fep_neuromod_type_t type,
    float amount
);

/**
 * @brief Set neuromodulator level directly
 *
 * WHAT: Override neuromodulator level
 * WHY:  Manual control for testing or external modulation
 * HOW:  Set level, clamp to bounds
 *
 * @param sys Neuromodulator system
 * @param type Neuromodulator type
 * @param level New level [0,1]
 * @return 0 on success, negative on error
 */
int fep_neuromod_set_level(
    fep_neuromod_system_t* sys,
    fep_neuromod_type_t type,
    float level
);

/**
 * @brief Get current neuromodulator level
 *
 * WHAT: Query current level
 * WHY:  Inspect state, log, or debug
 * HOW:  Return level from state array
 *
 * @param sys Neuromodulator system
 * @param type Neuromodulator type
 * @return Current level [0,1], -1.0f on error
 */
float fep_neuromod_get_level(
    const fep_neuromod_system_t* sys,
    fep_neuromod_type_t type
);

/* ============================================================================
 * Precision Modulation API
 * ============================================================================ */

/**
 * @brief Compute precision multiplier
 *
 * WHAT: Calculate precision weight from neuromodulator levels
 * WHY:  Precision-weighted prediction errors are core to FEP
 * HOW:  Π = base * (1 + ACh*gain_ach) * (1 + NE*gain_ne)
 *
 * BIOLOGICAL BASIS:
 * - ACh increases precision when predictions are reliable
 * - NE increases precision when events are surprising
 * - Combined effect is multiplicative
 *
 * @param sys Neuromodulator system
 * @param base_precision Base precision from sensory model
 * @return Modulated precision value
 */
float fep_neuromod_compute_precision(
    fep_neuromod_system_t* sys,
    float base_precision
);

/**
 * @brief Apply neuromodulation to FEP system
 *
 * WHAT: Update FEP precision based on neuromodulator state
 * WHY:  Bidirectional integration - neuromodulators affect FEP
 * HOW:  Compute precision, set in FEP system
 *
 * @param neuromod Neuromodulator system
 * @param fep FEP system to modulate
 * @return 0 on success, negative on error
 */
int fep_neuromod_apply_to_fep(
    fep_neuromod_system_t* neuromod,
    fep_system_t* fep
);

/* ============================================================================
 * Event-Driven Release API
 * ============================================================================ */

/**
 * @brief Handle prediction error event
 *
 * WHAT: Modulate ACh based on prediction error magnitude
 * WHY:  Large errors indicate unreliable predictions → decrease ACh
 * HOW:  ACh -= gain * normalized_error
 *
 * BIOLOGICAL BASIS: ACh signals expected uncertainty
 *
 * @param sys Neuromodulator system
 * @param error_magnitude Prediction error magnitude
 * @return 0 on success
 */
int fep_neuromod_on_prediction_error(
    fep_neuromod_system_t* sys,
    float error_magnitude
);

/**
 * @brief Handle surprise event
 *
 * WHAT: Release NE in response to surprising observation
 * WHY:  Surprise indicates salient, unexpected event → increase NE
 * HOW:  NE += gain * surprise
 *
 * BIOLOGICAL BASIS: NE signals unexpected uncertainty (salience)
 *
 * @param sys Neuromodulator system
 * @param surprise Surprise value (negative log probability)
 * @return 0 on success
 */
int fep_neuromod_on_surprise(
    fep_neuromod_system_t* sys,
    float surprise
);

/**
 * @brief Handle reward event
 *
 * WHAT: Release DA based on reward prediction error
 * WHY:  Unexpected reward → positive DA, omitted reward → negative DA
 * HOW:  DA += gain * (actual_reward - expected_reward)
 *
 * BIOLOGICAL BASIS: DA signals reward prediction error
 *
 * @param sys Neuromodulator system
 * @param reward Reward value (can be positive or negative)
 * @return 0 on success
 */
int fep_neuromod_on_reward(
    fep_neuromod_system_t* sys,
    float reward
);

/**
 * @brief Handle uncertainty event
 *
 * WHAT: Modulate ACh based on epistemic uncertainty
 * WHY:  High uncertainty → predictions unreliable → decrease ACh
 * HOW:  ACh += gain * (1 - uncertainty)
 *
 * BIOLOGICAL BASIS: ACh signals confidence in generative model
 *
 * @param sys Neuromodulator system
 * @param uncertainty Epistemic uncertainty [0,1]
 * @return 0 on success
 */
int fep_neuromod_on_uncertainty(
    fep_neuromod_system_t* sys,
    float uncertainty
);

/* ============================================================================
 * State Query API
 * ============================================================================ */

/**
 * @brief Get current neuromodulator state
 *
 * WHAT: Copy current state to output structure
 * WHY:  Inspect all neuromodulator levels and effects
 * HOW:  Memcpy state with mutex lock
 *
 * @param sys Neuromodulator system
 * @param state Output state structure
 * @return 0 on success, negative on error
 */
int fep_neuromod_get_state(
    const fep_neuromod_system_t* sys,
    fep_neuromod_state_t* state
);

/* ============================================================================
 * FEP Integration API
 * ============================================================================ */

/**
 * @brief Connect neuromodulator system to FEP system
 *
 * WHAT: Establish bidirectional link between neuromod and FEP
 * WHY:  Enable neuromodulators to read FEP state and modulate precision
 * HOW:  Store FEP pointer, register bio-async handlers
 *
 * @param neuromod Neuromodulator system
 * @param fep FEP system to connect
 * @return 0 on success, negative on error
 */
int fep_neuromod_connect(
    fep_neuromod_system_t* neuromod,
    fep_system_t* fep
);

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register with bio-async messaging system
 * WHY:  Enable inter-module communication for FEP events
 * HOW:  Register module with BIO_MODULE_FEP_NEUROMOD
 *
 * @param sys Neuromodulator system
 * @return 0 on success, negative on error
 */
int fep_neuromod_connect_bio_async(fep_neuromod_system_t* sys);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async system
 * WHY:  Clean shutdown
 * HOW:  Unregister module, clear context
 *
 * @param sys Neuromodulator system
 * @return 0 on success
 */
int fep_neuromod_disconnect_bio_async(fep_neuromod_system_t* sys);

/**
 * @brief Check if connected to bio-async
 *
 * @param sys Neuromodulator system
 * @return true if connected, false otherwise
 */
bool fep_neuromod_is_bio_async_connected(const fep_neuromod_system_t* sys);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert neuromodulator type to string
 *
 * @param type Neuromodulator type
 * @return Human-readable string
 */
const char* fep_neuromod_type_to_string(fep_neuromod_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_NEUROMOD_H */
