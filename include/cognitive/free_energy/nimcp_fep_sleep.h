/**
 * @file nimcp_fep_sleep.h
 * @brief Sleep-Dependent Memory Consolidation for Free Energy Principle
 * @version 1.0.0
 * @date 2025-12-12
 *
 * WHAT: Sleep cycle integration with FEP for memory consolidation and model optimization
 * WHY:  Sleep is essential for synaptic homeostasis, memory consolidation, and
 *       generative model refinement - replaying experiences to optimize predictions.
 * HOW:  Model sleep stages (NREM/REM), implement replay-based belief updating,
 *       and synaptic homeostasis (downscaling) for efficient encoding.
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * SLEEP STAGES:
 * -------------
 * - N1 (Light Sleep): Transition, theta waves, reduced sensory precision
 * - N2 (Stable Sleep): Sleep spindles, K-complexes, memory tagging
 * - N3 (Deep Sleep/SWS): Delta waves, synaptic downscaling, consolidation
 * - REM (Paradoxical Sleep): Dreaming, hippocampal-cortical transfer, creativity
 *
 * SYNAPTIC HOMEOSTASIS HYPOTHESIS (Tononi & Cirelli):
 * ---------------------------------------------------
 * Wake: Synaptic potentiation (learning) → net synaptic strength increases
 * Sleep: Synaptic downscaling → renormalization, noise removal
 * Result: Efficient encoding, preserved signal-to-noise ratio
 *
 * TWO-STAGE MEMORY CONSOLIDATION:
 * -------------------------------
 * 1. Hippocampal Replay (SWS): Sharp-wave ripples replay recent experiences
 * 2. Cortical Integration (REM): Hippocampal traces → cortical generative model
 *
 * FEP INTERPRETATION:
 * -------------------
 * Sleep serves to optimize the generative model:
 * - Replay minimizes expected free energy over accumulated experiences
 * - Synaptic downscaling implements precision normalization
 * - REM integrates episodic into semantic knowledge (model parameters)
 *
 * ARCHITECTURE:
 * ```
 * ╔═══════════════════════════════════════════════════════════════════════════╗
 * ║                    FEP SLEEP CONSOLIDATION SYSTEM                          ║
 * ╠═══════════════════════════════════════════════════════════════════════════╣
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   SLEEP STAGES                                      │  ║
 * ║   │                                                                     │  ║
 * ║   │   WAKE ──→ N1 ──→ N2 ──→ N3 (SWS) ──→ REM ──→ N2 ──→ ...         │  ║
 * ║   │     │       │      │        │           │                          │  ║
 * ║   │     │       │      │        │           │                          │  ║
 * ║   │     ▼       ▼      ▼        ▼           ▼                          │  ║
 * ║   │   Learn   Reduce  Tag    Replay+      Integrate                    │  ║
 * ║   │           Precis  Memory Downscale    Hippocampal→Cortical         │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   SWS CONSOLIDATION                                 │  ║
 * ║   │                                                                     │  ║
 * ║   │   Experience Buffer → Replay → Model Update → Downscale            │  ║
 * ║   │                                                                     │  ║
 * ║   │   For each replay:                                                  │  ║
 * ║   │     1. Sample experience (s_t, o_t, s_{t+1})                       │  ║
 * ║   │     2. Compute prediction error                                     │  ║
 * ║   │     3. Update transition/likelihood matrices                        │  ║
 * ║   │     4. Apply precision downscaling                                  │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ║   ┌────────────────────────────────────────────────────────────────────┐  ║
 * ║   │                   REM INTEGRATION                                   │  ║
 * ║   │                                                                     │  ║
 * ║   │   - Creative recombination of experiences                          │  ║
 * ║   │   - Abstract rule extraction                                        │  ║
 * ║   │   - Emotional memory processing                                     │  ║
 * ║   │   - Model generalization                                            │  ║
 * ║   └────────────────────────────────────────────────────────────────────┘  ║
 * ║                                                                            ║
 * ╚═══════════════════════════════════════════════════════════════════════════╝
 * ```
 *
 * REFERENCES:
 * - Tononi, G. & Cirelli, C. (2014) "Sleep and the price of plasticity"
 * - Diekelmann, S. & Born, J. (2010) "The memory function of sleep"
 * - Friston et al. (2017) "The graphical brain: Belief propagation and active inference"
 * - Walker, M. (2017) "Why We Sleep"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_FEP_SLEEP_H
#define NIMCP_FEP_SLEEP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "cognitive/free_energy/nimcp_free_energy.h"
#include "utils/validation/nimcp_common.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Sleep stage durations (typical, in ms) */
#define FEP_SLEEP_N1_DURATION_MS      300000   /**< ~5 minutes */
#define FEP_SLEEP_N2_DURATION_MS      1200000  /**< ~20 minutes */
#define FEP_SLEEP_SWS_DURATION_MS     1800000  /**< ~30 minutes */
#define FEP_SLEEP_REM_DURATION_MS     600000   /**< ~10 minutes (increases) */

/* Consolidation parameters */
#define FEP_SLEEP_DEFAULT_REPLAY_COUNT     100   /**< Replays per SWS cycle */
#define FEP_SLEEP_DEFAULT_DOWNSCALE_FACTOR 0.9f  /**< Synaptic downscaling */
#define FEP_SLEEP_DEFAULT_BUFFER_SIZE      1000  /**< Experience buffer size */

/* Precision modulation */
#define FEP_SLEEP_WAKE_PRECISION           1.0f  /**< Full precision awake */
#define FEP_SLEEP_N1_PRECISION             0.8f  /**< Reduced in N1 */
#define FEP_SLEEP_N2_PRECISION             0.5f  /**< Further reduced in N2 */
#define FEP_SLEEP_SWS_PRECISION            0.2f  /**< Minimal in SWS */
#define FEP_SLEEP_REM_PRECISION            0.6f  /**< Partial in REM */

/* FEP → Sleep: Sleep pressure modulation constants */
#define FEP_SLEEP_PE_PRESSURE_GAIN         0.1f   /**< PE contribution to sleep pressure */
#define FEP_SLEEP_UNCERTAINTY_PRESSURE     0.15f  /**< Uncertainty contribution to pressure */
#define FEP_SLEEP_CONVERGENCE_THRESHOLD    0.01f  /**< PE threshold for convergence */
#define FEP_SLEEP_MAX_PRESSURE             1.0f   /**< Maximum sleep pressure */
#define FEP_SLEEP_MIN_PRESSURE             0.0f   /**< Minimum sleep pressure */
#define FEP_SLEEP_PRESSURE_DECAY           0.001f /**< Pressure decay per ms awake */
#define FEP_SLEEP_PRESSURE_THRESHOLD       0.7f   /**< Threshold for sleep recommendation */
#define FEP_SLEEP_HIGH_PE_THRESHOLD        0.5f   /**< Threshold for high prediction error */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sleep stages
 */
typedef enum {
    SLEEP_STAGE_WAKE = 0,   /**< Awake */
    SLEEP_STAGE_N1,         /**< Light sleep (transition) */
    SLEEP_STAGE_N2,         /**< Stable sleep (spindles) */
    SLEEP_STAGE_SWS,        /**< Slow-wave sleep (deep) */
    SLEEP_STAGE_REM         /**< Rapid eye movement */
} fep_sleep_stage_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Sleep system configuration
 */
typedef struct {
    /* Stage durations */
    uint32_t n1_duration_ms;
    uint32_t n2_duration_ms;
    uint32_t sws_duration_ms;
    uint32_t rem_duration_ms;

    /* Consolidation */
    uint32_t replays_per_cycle;
    float downscale_factor;
    size_t experience_buffer_size;

    /* Behavior */
    bool enable_auto_cycle;       /**< Automatic stage transitions */
    bool enable_synaptic_homeostasis;
    bool enable_replay_consolidation;
    bool enable_rem_integration;
} fep_sleep_config_t;

/**
 * @brief Single experience for replay
 */
typedef struct {
    float* state;                 /**< State at time t */
    float* observation;           /**< Observation at time t */
    float* next_state;            /**< State at time t+1 */
    uint32_t dim;                 /**< State dimensionality */
    uint32_t obs_dim;             /**< Observation dimensionality */
    float importance;             /**< Replay priority */
} fep_experience_t;

/**
 * @brief Sleep system state
 */
typedef struct {
    fep_sleep_stage_t current_stage;
    uint64_t stage_start_ms;
    uint64_t total_sleep_ms;
    uint32_t cycle_count;         /**< Number of complete cycles */

    /* Consolidation progress */
    uint32_t replays_this_cycle;
    float consolidation_progress;
    float total_downscaling;

    /* Quality metrics */
    float sleep_efficiency;       /**< Time asleep / time in bed */
    float consolidation_quality;  /**< Model improvement during sleep */
} fep_sleep_state_t;

/**
 * @brief Sleep system statistics
 */
typedef struct {
    uint64_t total_wake_time_ms;
    uint64_t total_n1_time_ms;
    uint64_t total_n2_time_ms;
    uint64_t total_sws_time_ms;
    uint64_t total_rem_time_ms;

    uint32_t total_cycles;
    uint64_t total_replays;
    float avg_downscaling;
    float model_improvement;
} fep_sleep_stats_t;

/**
 * @brief FEP-driven sleep pressure state (FEP → Sleep direction)
 *
 * WHAT: Sleep need indicators derived from FEP state
 * WHY:  FEP prediction errors and uncertainty affect sleep need
 * HOW:  Track accumulated prediction errors, uncertainty, convergence
 */
typedef struct {
    float sleep_pressure;              /**< Accumulated sleep need [0,1] */
    float accumulated_prediction_error; /**< Running sum of prediction errors */
    float avg_uncertainty;             /**< Average epistemic uncertainty */
    bool model_converged;              /**< Has FEP model converged? */
    float convergence_quality;         /**< Quality of convergence [0,1] */
    uint64_t wake_duration_ms;         /**< Time awake since last sleep */
    uint32_t high_pe_events;           /**< Count of high prediction error events */
    bool sleep_recommended;            /**< Is sleep currently recommended? */
} fep_sleep_pressure_t;

/**
 * @brief Complete FEP sleep system
 */
typedef struct fep_sleep_system {
    /* Configuration */
    fep_sleep_config_t config;

    /* Connected FEP system */
    fep_system_t* fep_system;

    /* Current state */
    fep_sleep_state_t state;

    /* FEP → Sleep: Sleep pressure tracking */
    fep_sleep_pressure_t pressure;

    /* Experience buffer */
    fep_experience_t* experience_buffer;
    size_t buffer_count;
    size_t buffer_capacity;

    /* Statistics */
    fep_sleep_stats_t stats;

    /* Bio-async */
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    /* Thread safety */
    nimcp_mutex_t* mutex;
} fep_sleep_system_t;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * @brief Get default sleep configuration
 *
 * @param config Output configuration
 */
void fep_sleep_default_config(fep_sleep_config_t* config);

/**
 * @brief Create sleep system
 *
 * @param config Configuration (NULL for defaults)
 * @return New sleep system or NULL on failure
 */
fep_sleep_system_t* fep_sleep_create(const fep_sleep_config_t* config);

/**
 * @brief Destroy sleep system
 *
 * @param sys Sleep system (NULL safe)
 */
void fep_sleep_destroy(fep_sleep_system_t* sys);

/* ============================================================================
 * Sleep Stage API
 * ============================================================================ */

/**
 * @brief Transition to specific sleep stage
 *
 * @param sys Sleep system
 * @param stage Target stage
 * @return 0 on success
 */
int fep_sleep_set_stage(fep_sleep_system_t* sys, fep_sleep_stage_t stage);

/**
 * @brief Get current sleep stage
 *
 * @param sys Sleep system
 * @return Current stage
 */
fep_sleep_stage_t fep_sleep_get_stage(const fep_sleep_system_t* sys);

/**
 * @brief Update sleep system (automatic stage progression)
 *
 * @param sys Sleep system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int fep_sleep_update(fep_sleep_system_t* sys, uint64_t delta_ms);

/* ============================================================================
 * Consolidation API
 * ============================================================================ */

/**
 * @brief Add experience to replay buffer
 *
 * @param sys Sleep system
 * @param state Current state
 * @param observation Current observation
 * @param next_state Next state
 * @param dim State dimensionality
 * @param obs_dim Observation dimensionality
 * @return 0 on success
 */
int fep_sleep_add_experience(
    fep_sleep_system_t* sys,
    const float* state,
    const float* observation,
    const float* next_state,
    size_t dim,
    size_t obs_dim
);

/**
 * @brief Run replay consolidation (during SWS)
 *
 * @param sys Sleep system
 * @param fep FEP system to update
 * @param num_replays Number of experiences to replay
 * @return 0 on success
 */
int fep_sleep_replay_consolidation(
    fep_sleep_system_t* sys,
    fep_system_t* fep,
    uint32_t num_replays
);

/**
 * @brief Apply synaptic downscaling
 *
 * @param sys Sleep system
 * @param fep FEP system to downscale
 * @param factor Downscaling factor (0-1)
 * @return 0 on success
 */
int fep_sleep_apply_downscaling(
    fep_sleep_system_t* sys,
    fep_system_t* fep,
    float factor
);

/**
 * @brief Run REM integration
 *
 * @param sys Sleep system
 * @param fep FEP system
 * @return 0 on success
 */
int fep_sleep_rem_integration(
    fep_sleep_system_t* sys,
    fep_system_t* fep
);

/* ============================================================================
 * State/Stats API
 * ============================================================================ */

/**
 * @brief Get current state
 *
 * @param sys Sleep system
 * @param state Output state
 * @return 0 on success
 */
int fep_sleep_get_state(
    const fep_sleep_system_t* sys,
    fep_sleep_state_t* state
);

/**
 * @brief Get statistics
 *
 * @param sys Sleep system
 * @param stats Output statistics
 * @return 0 on success
 */
int fep_sleep_get_stats(
    const fep_sleep_system_t* sys,
    fep_sleep_stats_t* stats
);

/**
 * @brief Get precision modifier for current stage
 *
 * @param sys Sleep system
 * @return Precision modifier (0-1)
 */
float fep_sleep_get_precision_modifier(const fep_sleep_system_t* sys);

/* ============================================================================
 * FEP → Sleep Direction (Bidirectional Integration)
 * ============================================================================ */

/**
 * @brief Report prediction error to sleep system
 *
 * WHAT: Accumulate prediction errors to increase sleep pressure
 * WHY:  High prediction errors indicate cognitive fatigue → need sleep
 * HOW:  Add scaled PE to sleep pressure, track high PE events
 *
 * BIOLOGICAL BASIS:
 * - Cognitive effort during waking increases adenosine
 * - Prediction errors represent metabolic cost
 * - Accumulated errors trigger homeostatic sleep drive
 *
 * @param sys Sleep system
 * @param prediction_error Prediction error magnitude
 * @return 0 on success
 */
int fep_sleep_on_prediction_error(fep_sleep_system_t* sys, float prediction_error);

/**
 * @brief Report model uncertainty to sleep system
 *
 * WHAT: Update sleep pressure based on epistemic uncertainty
 * WHY:  High uncertainty indicates poor model quality → need consolidation
 * HOW:  Track average uncertainty, increase pressure when high
 *
 * @param sys Sleep system
 * @param uncertainty Current epistemic uncertainty [0,1]
 * @return 0 on success
 */
int fep_sleep_on_uncertainty(fep_sleep_system_t* sys, float uncertainty);

/**
 * @brief Report model convergence to sleep system
 *
 * WHAT: Signal that FEP model has converged
 * WHY:  Convergence indicates good time for consolidation
 * HOW:  Set convergence flag, may trigger sleep readiness
 *
 * @param sys Sleep system
 * @param converged Whether model has converged
 * @param convergence_quality Quality of convergence [0,1]
 * @return 0 on success
 */
int fep_sleep_on_convergence(fep_sleep_system_t* sys, bool converged, float convergence_quality);

/**
 * @brief Get current sleep pressure
 *
 * WHAT: Query accumulated sleep need
 * WHY:  External systems need to know when sleep is needed
 * HOW:  Return current sleep pressure value
 *
 * @param sys Sleep system
 * @return Sleep pressure [0,1], higher = more sleep needed
 */
float fep_sleep_get_pressure(const fep_sleep_system_t* sys);

/**
 * @brief Get full sleep pressure state
 *
 * WHAT: Query complete FEP-driven sleep pressure state
 * WHY:  Detailed state for monitoring and decision-making
 * HOW:  Copy internal pressure state to output
 *
 * @param sys Sleep system
 * @param pressure Output pressure state
 * @return 0 on success
 */
int fep_sleep_get_pressure_state(fep_sleep_system_t* sys, fep_sleep_pressure_t* pressure);

/**
 * @brief Check if sleep is recommended based on FEP state
 *
 * WHAT: Determine if sleep should occur based on accumulated FEP signals
 * WHY:  Provides recommendation for sleep scheduling
 * HOW:  Evaluate pressure threshold, uncertainty, convergence
 *
 * @param sys Sleep system
 * @return true if sleep is recommended
 */
bool fep_sleep_is_sleep_recommended(const fep_sleep_system_t* sys);

/**
 * @brief Reset sleep pressure after waking
 *
 * WHAT: Clear accumulated sleep pressure
 * WHY:  After adequate sleep, pressure should reset
 * HOW:  Zero out pressure state
 *
 * @param sys Sleep system
 * @return 0 on success
 */
int fep_sleep_reset_pressure(fep_sleep_system_t* sys);

/**
 * @brief Update sleep pressure based on wake duration
 *
 * WHAT: Increase sleep pressure over time awake
 * WHY:  Homeostatic sleep drive increases with wake time
 * HOW:  Add time-based component to pressure
 *
 * @param sys Sleep system
 * @param delta_ms Time since last update
 * @return 0 on success
 */
int fep_sleep_update_pressure(fep_sleep_system_t* sys, uint64_t delta_ms);

/* ============================================================================
 * Integration API
 * ============================================================================ */

/**
 * @brief Connect to FEP system
 *
 * @param sleep Sleep system
 * @param fep FEP system
 * @return 0 on success
 */
int fep_sleep_connect(fep_sleep_system_t* sleep, fep_system_t* fep);

/**
 * @brief Connect to bio-async router
 *
 * @param sys Sleep system
 * @return 0 on success
 */
int fep_sleep_connect_bio_async(fep_sleep_system_t* sys);

/**
 * @brief Disconnect from bio-async router
 *
 * @param sys Sleep system
 * @return 0 on success
 */
int fep_sleep_disconnect_bio_async(fep_sleep_system_t* sys);

/**
 * @brief Check if bio-async is connected
 *
 * @param sys Sleep system
 * @return true if connected
 */
bool fep_sleep_is_bio_async_connected(const fep_sleep_system_t* sys);

/* ============================================================================
 * String Conversion API
 * ============================================================================ */

/**
 * @brief Convert sleep stage to string
 *
 * @param stage Sleep stage
 * @return Human-readable string
 */
const char* fep_sleep_stage_to_string(fep_sleep_stage_t stage);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEP_SLEEP_H */
