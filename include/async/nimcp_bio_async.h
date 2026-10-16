/**
 * @file nimcp_bio_async.h
 * @brief Biologically-Inspired Asynchronous Computation System
 *
 * WHAT: Async coordination system based on biological signaling mechanisms
 * WHY:  Replace traditional futures/promises with brain-inspired patterns
 * HOW:  Neuromodulator channels, oscillation coupling, predictive coding, glial waves
 *
 * BIOLOGICAL MECHANISMS:
 *
 * 1. NEUROMODULATOR CHANNELS (Reward/Priority signaling)
 *    ┌────────────────────────────────────────────────────────┐
 *    │  Dopamine:     Goal completion, reward prediction      │
 *    │  Serotonin:    Mood/state coordination, patience       │
 *    │  Norepinephrine: Alertness, priority escalation        │
 *    │  Acetylcholine:  Attention, fast context switching     │
 *    └────────────────────────────────────────────────────────┘
 *
 * 2. PHASE COUPLING (Synchronized operations)
 *    ┌────────────────────────────────────────────────────────┐
 *    │  Kuramoto oscillators for multi-future synchronization │
 *    │  Cross-frequency coupling for hierarchical sync        │
 *    │  Phase coherence thresholds for "all ready" detection  │
 *    └────────────────────────────────────────────────────────┘
 *
 * 3. PREDICTIVE CODING (Error-driven callbacks)
 *    ┌────────────────────────────────────────────────────────┐
 *    │  Bayesian predictions with precision weighting         │
 *    │  Callbacks only fire on prediction errors              │
 *    │  Surprise-based event handling                         │
 *    └────────────────────────────────────────────────────────┘
 *
 * 4. GLIAL SIGNALING (System-wide coordination)
 *    ┌────────────────────────────────────────────────────────┐
 *    │  Calcium wave propagation for global state changes     │
 *    │  Metabolic/resource management signaling               │
 *    │  Slow but system-wide coordination                     │
 *    └────────────────────────────────────────────────────────┘
 *
 * USAGE EXAMPLE:
 * ```c
 * // Initialize bio-async system
 * nimcp_bio_async_config_t config = nimcp_bio_async_default_config();
 * nimcp_bio_async_init(&config);
 *
 * // Create neuromodulator-based promise (dopamine = reward/completion)
 * nimcp_bio_promise_t promise = nimcp_bio_promise_create(
 *     BIO_CHANNEL_DOPAMINE, sizeof(float));
 * nimcp_bio_future_t future = nimcp_bio_promise_get_future(promise);
 *
 * // Complete with biological dynamics (result decays over time)
 * float result = 42.0f;
 * nimcp_bio_promise_complete(promise, &result);
 *
 * // Wait with biological timeout (follows neuromodulator decay)
 * nimcp_bio_future_wait(future, &result, 0);  // No timeout
 * float confidence = nimcp_bio_future_get_confidence(future);  // Decays!
 *
 * // Phase-coupled synchronization (biological "all")
 * nimcp_phase_sync_t sync = nimcp_phase_sync_create(BIO_OSC_GAMMA);
 * nimcp_phase_sync_add_future(sync, future1);
 * nimcp_phase_sync_add_future(sync, future2);
 * nimcp_phase_sync_wait_coherent(sync, 0.8f);  // Wait for 80% coherence
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-11-28
 * @version 1.0.0
 */

#ifndef NIMCP_BIO_ASYNC_H
#define NIMCP_BIO_ASYNC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "async/nimcp_biological_timescales.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct nimcp_bio_promise_struct* nimcp_bio_promise_t;
typedef struct nimcp_bio_future_struct* nimcp_bio_future_t;
typedef struct nimcp_phase_sync_struct* nimcp_phase_sync_t;
typedef struct nimcp_glial_wave_struct* nimcp_glial_wave_t;
typedef struct nimcp_predictive_model_struct* nimcp_predictive_model_t;

//=============================================================================
// Error Codes (bio-async specific)
//=============================================================================

/* Include canonical error type definition */
#ifndef NIMCP_ERROR_TYPE_DEFINED
#include "utils/error/nimcp_error_codes.h"
#endif

#define NIMCP_BIO_ERROR_BASE 10000
#define NIMCP_BIO_ERROR_NOT_INITIALIZED      (NIMCP_BIO_ERROR_BASE + 1)
#define NIMCP_BIO_ERROR_INVALID_CHANNEL      (NIMCP_BIO_ERROR_BASE + 2)
#define NIMCP_BIO_ERROR_CHANNEL_SATURATED    (NIMCP_BIO_ERROR_BASE + 3)
#define NIMCP_BIO_ERROR_PHASE_INCOHERENT     (NIMCP_BIO_ERROR_BASE + 4)
#define NIMCP_BIO_ERROR_WAVE_EXTINCT         (NIMCP_BIO_ERROR_BASE + 5)
#define NIMCP_BIO_ERROR_PREDICTION_STALE     (NIMCP_BIO_ERROR_BASE + 6)
#define NIMCP_BIO_ERROR_REFRACTORY           (NIMCP_BIO_ERROR_BASE + 7)
#define NIMCP_BIO_ERROR_DECAY_COMPLETE       (NIMCP_BIO_ERROR_BASE + 8)

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Neuromodulator channel types
 *
 * Each channel has different timing characteristics and semantics:
 * - DOPAMINE: Fast reward/completion signal, medium decay
 * - SEROTONIN: Slow mood/state signal, long decay
 * - NOREPINEPHRINE: Fast alerting signal, medium decay
 * - ACETYLCHOLINE: Very fast attention signal, short decay
 */
typedef enum {
    BIO_CHANNEL_DOPAMINE = 0,       /**< DA: Reward, goal completion */
    BIO_CHANNEL_SEROTONIN = 1,      /**< 5-HT: Mood, state coordination */
    BIO_CHANNEL_NOREPINEPHRINE = 2, /**< NE: Alertness, priority */
    BIO_CHANNEL_ACETYLCHOLINE = 3,  /**< ACh: Attention, fast switching */
    BIO_CHANNEL_COUNT = 4
} nimcp_bio_channel_type_t;

/**
 * @brief Neural oscillation bands for phase coupling
 */
typedef enum {
    BIO_OSC_DELTA = 0,  /**< 0.5-4 Hz: Deep coordination, homeostatic */
    BIO_OSC_THETA = 1,  /**< 4-8 Hz: Memory, sequential processing */
    BIO_OSC_ALPHA = 2,  /**< 8-12 Hz: Attention, inhibitory gating */
    BIO_OSC_BETA = 3,   /**< 12-30 Hz: Motor/working memory */
    BIO_OSC_GAMMA = 4,  /**< 30-100 Hz: Fast binding, consciousness */
    BIO_OSC_BAND_COUNT = 5
} nimcp_oscillation_band_t;

/**
 * @brief Bio-future states (extends traditional states)
 */
typedef enum {
    BIO_FUTURE_PENDING = 0,      /**< Waiting for completion */
    BIO_FUTURE_COMPLETED = 1,    /**< Successfully completed */
    BIO_FUTURE_FAILED = 2,       /**< Failed with error */
    BIO_FUTURE_CANCELLED = 3,    /**< Cancelled by user */
    BIO_FUTURE_DECAYED = 4,      /**< Result decayed below threshold */
    BIO_FUTURE_REFRACTORY = 5    /**< In refractory period */
} nimcp_bio_future_state_t;

/**
 * @brief Glial wave propagation modes
 */
typedef enum {
    BIO_WAVE_ISOTROPIC = 0,      /**< Uniform radial propagation */
    BIO_WAVE_ANISOTROPIC = 1,    /**< Direction-dependent propagation */
    BIO_WAVE_NETWORK = 2         /**< Along network topology */
} nimcp_wave_mode_t;

//=============================================================================
// Configuration Structures
//=============================================================================

/**
 * @brief Configuration for a neuromodulator channel
 */
typedef struct {
    float baseline_concentration;   /**< Baseline level (μM) */
    float peak_concentration;       /**< Peak after release (μM) */
    float decay_tau_ms;             /**< Decay time constant */
    float diffusion_coef;           /**< Diffusion coefficient (μm²/ms) */
    float refractory_period_ms;     /**< Minimum time between bursts */
    bool enable_diffusion;          /**< Enable spatial diffusion model */
} nimcp_channel_config_t;

/**
 * @brief Configuration for phase coupling system
 */
typedef struct {
    float coherence_threshold;      /**< Threshold for "synchronized" */
    float coupling_strength;        /**< Kuramoto K parameter */
    float frequency_spread;         /**< Natural frequency variance */
    uint32_t max_oscillators;       /**< Maximum coupled oscillators */
    bool enable_cross_frequency;    /**< Enable cross-frequency coupling */
} nimcp_phase_config_t;

/**
 * @brief Configuration for predictive coding
 */
typedef struct {
    float default_prior_precision;  /**< Default π_prior */
    float default_likelihood_precision; /**< Default π_likelihood */
    float learning_rate;            /**< Precision adaptation rate */
    float surprise_threshold;       /**< Threshold for callback */
    uint32_t max_predictors;        /**< Maximum tracked predictions */
} nimcp_predictive_config_t;

/**
 * @brief Configuration for glial signaling
 */
typedef struct {
    float wave_speed_um_s;          /**< Calcium wave speed */
    float wave_threshold_um;        /**< Activation threshold */
    float decay_rate;               /**< Wave amplitude decay */
    uint32_t max_concurrent_waves;  /**< Maximum simultaneous waves */
    nimcp_wave_mode_t mode;         /**< Propagation mode */
} nimcp_glial_config_t;

/**
 * @brief Main bio-async system configuration
 */
typedef struct {
    /* Subsystem configurations */
    nimcp_channel_config_t channel_configs[BIO_CHANNEL_COUNT];
    nimcp_phase_config_t phase_config;
    nimcp_predictive_config_t predictive_config;
    nimcp_glial_config_t glial_config;

    /* Threading */
    uint32_t thread_pool_size;      /**< Worker threads (0 = auto) */
    bool enable_thread_affinity;    /**< Pin workers to cores */

    /* Memory */
    size_t max_memory_bytes;        /**< Memory limit (0 = unlimited) */
    bool use_unified_memory;        /**< Use NIMCP unified memory */

    /* Timing */
    float simulation_dt_ms;         /**< Update timestep */
    bool use_real_time;             /**< Real-time vs accelerated */
    float time_acceleration;        /**< Simulation speedup factor */

    /* Debug */
    bool enable_statistics;         /**< Track performance stats */
    bool enable_logging;            /**< Debug logging */
} nimcp_bio_async_config_t;

//=============================================================================
// Statistics Structures
//=============================================================================

/**
 * @brief Per-channel statistics
 */
typedef struct {
    uint64_t releases;              /**< Total release events */
    uint64_t refractory_blocks;     /**< Blocked by refractory */
    float avg_peak_concentration;   /**< Average peak level */
    float avg_decay_time_ms;        /**< Average time to baseline */
    uint64_t active_futures;        /**< Currently active futures */
} nimcp_channel_stats_t;

/**
 * @brief Phase coupling statistics
 */
typedef struct {
    uint64_t sync_requests;         /**< Total sync operations */
    uint64_t sync_achieved;         /**< Successful synchronizations */
    uint64_t sync_timeouts;         /**< Timeout before coherence */
    float avg_coherence;            /**< Average achieved coherence */
    float avg_sync_time_ms;         /**< Average time to sync */
} nimcp_phase_stats_t;

/**
 * @brief Predictive coding statistics
 */
typedef struct {
    uint64_t predictions_made;      /**< Total predictions */
    uint64_t callbacks_triggered;   /**< Callbacks fired (errors) */
    uint64_t callbacks_suppressed;  /**< Callbacks suppressed (correct) */
    float avg_surprise;             /**< Average surprise level */
    float avg_precision;            /**< Average precision */
} nimcp_predictive_stats_t;

/**
 * @brief Complete bio-async system statistics
 */
typedef struct {
    nimcp_channel_stats_t channel_stats[BIO_CHANNEL_COUNT];
    nimcp_phase_stats_t phase_stats;
    nimcp_predictive_stats_t predictive_stats;

    /* Overall metrics */
    uint64_t total_futures_created;
    uint64_t total_futures_completed;
    uint64_t total_futures_decayed;
    size_t current_memory_bytes;
    uint64_t simulation_steps;
    float simulation_time_ms;
} nimcp_bio_async_stats_t;

//=============================================================================
// Callback Types
//=============================================================================

/**
 * @brief Callback for bio-future completion
 *
 * @param result Pointer to result (NULL if failed/decayed)
 * @param confidence Current confidence level [0,1] (decays over time)
 * @param error Error code (NIMCP_SUCCESS if ok)
 * @param user_data User context
 */
typedef void (*nimcp_bio_callback_t)(
    const void* result,
    float confidence,
    nimcp_error_t error,
    void* user_data
);

/**
 * @brief Callback for prediction error (predictive coding)
 *
 * @param signal_name Name of the predicted signal
 * @param prediction Expected value
 * @param actual Observed value
 * @param error Precision-weighted prediction error
 * @param surprise Surprise level (negative log probability)
 * @param user_data User context
 */
typedef void (*nimcp_prediction_error_callback_t)(
    const char* signal_name,
    float prediction,
    float actual,
    float error,
    float surprise,
    void* user_data
);

/**
 * @brief Callback for glial wave arrival
 *
 * @param wave Wave handle
 * @param region_id Affected region
 * @param calcium_level Local calcium concentration
 * @param user_data User context
 */
typedef void (*nimcp_wave_callback_t)(
    nimcp_glial_wave_t wave,
    uint32_t region_id,
    float calcium_level,
    void* user_data
);

//=============================================================================
// Module Initialization API
//=============================================================================

/**
 * @brief Get default bio-async configuration
 *
 * Returns configuration with biologically-realistic defaults.
 * Computational scaling is applied for real-time performance.
 *
 * @return Default configuration structure
 */
nimcp_bio_async_config_t nimcp_bio_async_default_config(void);

/**
 * @brief Initialize bio-async system
 *
 * WHAT: Initializes all bio-async subsystems
 * WHY:  Must be called before using any bio-async functions
 * HOW:  Sets up thread pool, memory, all subsystem state
 *
 * @param config Configuration (NULL for defaults)
 * @return NIMCP_SUCCESS or error code
 *
 * THREAD SAFETY: Must be called from single thread
 */
nimcp_error_t nimcp_bio_async_init(const nimcp_bio_async_config_t* config);

/**
 * @brief Shutdown bio-async system
 *
 * @note All futures/promises should be destroyed first
 */
void nimcp_bio_async_shutdown(void);

/**
 * @brief Check if bio-async is initialized
 */
bool nimcp_bio_async_is_initialized(void);

/**
 * @brief Get bio-async statistics
 *
 * @param stats Output statistics structure
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bio_async_get_stats(nimcp_bio_async_stats_t* stats);

/**
 * @brief Reset statistics counters
 */
void nimcp_bio_async_reset_stats(void);

/**
 * @brief Advance simulation time by one step
 *
 * WHAT: Updates all biological dynamics
 * WHY:  Advance decay, diffusion, oscillations
 * HOW:  Called automatically or manually for testing
 *
 * @param dt_ms Time step in milliseconds (0 = use config default)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bio_async_step(float dt_ms);

//=============================================================================
// Bio-Promise/Future API (Neuromodulator-based)
//=============================================================================

/**
 * @brief Create bio-promise with neuromodulator semantics
 *
 * WHAT: Creates promise that signals via neuromodulator dynamics
 * WHY:  Biologically-realistic completion signaling with decay
 * HOW:  Allocates state, associates with channel, tracks concentration
 *
 * @param channel Neuromodulator channel type
 * @param result_size Size of result data in bytes
 * @return Promise handle or NULL on failure
 *
 * CHANNEL SEMANTICS:
 * - DOPAMINE: Fast completion (~3ms peak, ~2s decay), high confidence
 * - SEROTONIN: Slow state change (~10s decay), sustained signal
 * - NOREPINEPHRINE: Alerting (~100ms phasic, ~3s decay), priority
 * - ACETYLCHOLINE: Immediate attention (~50ms decay), fast switch
 */
nimcp_bio_promise_t nimcp_bio_promise_create(
    nimcp_bio_channel_type_t channel,
    size_t result_size
);

/**
 * @brief Complete bio-promise with result
 *
 * WHAT: Triggers neuromodulator release with result
 * WHY:  Signal completion to all futures with biological dynamics
 * HOW:  Sets result, triggers concentration spike, starts decay
 *
 * @param promise Promise handle
 * @param result Result data pointer
 * @return NIMCP_SUCCESS or error code
 *
 * ERRORS:
 * - NIMCP_BIO_ERROR_REFRACTORY: Channel in refractory period
 * - NIMCP_BIO_ERROR_CHANNEL_SATURATED: Concentration at maximum
 */
nimcp_error_t nimcp_bio_promise_complete(
    nimcp_bio_promise_t promise,
    const void* result
);

/**
 * @brief Complete bio-promise with sized result
 *
 * Use this when the promise was created with result_size=0 (unknown size)
 * and the handler needs to provide the actual result size.
 *
 * @param promise Promise handle
 * @param result Result data to copy
 * @param result_size Size of result data in bytes
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bio_promise_complete_sized(
    nimcp_bio_promise_t promise,
    const void* result,
    size_t result_size
);

/**
 * @brief Fail bio-promise with error
 *
 * @param promise Promise handle
 * @param error Error code
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_bio_promise_fail(
    nimcp_bio_promise_t promise,
    nimcp_error_t error
);

/**
 * @brief Get future from bio-promise
 *
 * @param promise Promise handle
 * @return Future handle or NULL on failure
 */
nimcp_bio_future_t nimcp_bio_promise_get_future(nimcp_bio_promise_t promise);

/**
 * @brief Destroy bio-promise
 *
 * @param promise Promise handle (NULL safe)
 */
void nimcp_bio_promise_destroy(nimcp_bio_promise_t promise);

/**
 * @brief Get bio-future state
 *
 * @param future Future handle
 * @return Current state
 */
nimcp_bio_future_state_t nimcp_bio_future_state(nimcp_bio_future_t future);

/**
 * @brief Wait for bio-future with biological dynamics
 *
 * WHAT: Blocks until completion or decay below threshold
 * WHY:  Biological waiting with confidence tracking
 * HOW:  Monitors concentration, returns when ready or decayed
 *
 * @param future Future handle
 * @param out_result Output buffer for result
 * @param timeout_ms Timeout (0 = wait for decay)
 * @return NIMCP_SUCCESS if completed, error if decayed/failed/timeout
 *
 * NOTE: Even after completion, confidence decays. Check get_confidence().
 */
nimcp_error_t nimcp_bio_future_wait(
    nimcp_bio_future_t future,
    void* out_result,
    uint64_t timeout_ms
);

/**
 * @brief Get current confidence level
 *
 * WHAT: Returns concentration-based confidence [0,1]
 * WHY:  Biological signals decay - confidence indicates freshness
 * HOW:  Maps neuromodulator concentration to [0,1] range
 *
 * @param future Future handle
 * @return Confidence level (1.0 = just completed, 0.0 = fully decayed)
 *
 * FORMULA: confidence = (concentration - baseline) / (peak - baseline)
 */
float nimcp_bio_future_get_confidence(nimcp_bio_future_t future);

/**
 * @brief Check if future is ready (non-blocking)
 *
 * @param future Future handle
 * @return true if completed/failed/cancelled/decayed
 */
bool nimcp_bio_future_is_ready(nimcp_bio_future_t future);

/**
 * @brief Get time since completion (ms)
 *
 * @param future Future handle
 * @return Time since completion, or -1 if not completed
 */
float nimcp_bio_future_get_age_ms(nimcp_bio_future_t future);

/**
 * @brief Register callback for bio-future
 *
 * WHAT: Register callback invoked on completion
 * WHY:  Async notification with confidence info
 * HOW:  Called immediately if already ready, else queued
 *
 * @param future Future handle
 * @param callback Callback function
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 *
 * NOTE: Callback receives current confidence at call time
 */
nimcp_error_t nimcp_bio_future_then(
    nimcp_bio_future_t future,
    nimcp_bio_callback_t callback,
    void* user_data
);

/**
 * @brief Cancel bio-future
 *
 * @param future Future handle
 * @return true if cancelled, false if already completed
 */
bool nimcp_bio_future_cancel(nimcp_bio_future_t future);

/**
 * @brief Destroy bio-future
 *
 * @param future Future handle (NULL safe)
 */
void nimcp_bio_future_destroy(nimcp_bio_future_t future);

//=============================================================================
// Phase Coupling API (Oscillation-based synchronization)
//=============================================================================

/**
 * @brief Create phase synchronization context
 *
 * WHAT: Creates Kuramoto-based synchronization group
 * WHY:  Biological alternative to future_all()
 * HOW:  Oscillators with coupling, coherence detection
 *
 * @param band Oscillation band (determines frequency and coupling)
 * @return Sync handle or NULL on failure
 *
 * BAND SELECTION:
 * - DELTA: Slow sync, tolerates long delays
 * - THETA: Memory/sequence coordination
 * - ALPHA: Attention-gated sync
 * - BETA: Working memory coordination
 * - GAMMA: Fast binding, tight sync required
 */
nimcp_phase_sync_t nimcp_phase_sync_create(nimcp_oscillation_band_t band);

/**
 * @brief Add bio-future to phase sync group
 *
 * WHAT: Adds oscillator for this future
 * WHY:  Track phase of multiple futures
 * HOW:  Creates oscillator with natural frequency + noise
 *
 * @param sync Sync handle
 * @param future Future to track
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_phase_sync_add_future(
    nimcp_phase_sync_t sync,
    nimcp_bio_future_t future
);

/**
 * @brief Wait for all futures to reach phase coherence
 *
 * WHAT: Blocks until oscillators synchronize
 * WHY:  Biological "all" - completion when phases align
 * HOW:  Monitors Kuramoto order parameter r
 *
 * @param sync Sync handle
 * @param timeout_ms Timeout (0 = use band-appropriate default)
 * @return NIMCP_SUCCESS if coherent, error code otherwise
 *
 * DEFAULT TIMEOUTS (cycles of band frequency):
 * - DELTA: ~10 cycles = ~5000ms
 * - THETA: ~10 cycles = ~1667ms
 * - ALPHA: ~10 cycles = ~1000ms
 * - BETA: ~10 cycles = ~500ms
 * - GAMMA: ~10 cycles = ~250ms
 */
nimcp_error_t nimcp_phase_sync_wait_all(
    nimcp_phase_sync_t sync,
    uint64_t timeout_ms
);

/**
 * @brief Wait for specified coherence level
 *
 * WHAT: Blocks until order parameter r >= threshold
 * WHY:  Allow partial synchronization
 * HOW:  Monitors r = |Σ exp(iθ)/N|
 *
 * @param sync Sync handle
 * @param coherence_threshold Required r value [0,1]
 * @param timeout_ms Timeout
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_phase_sync_wait_coherent(
    nimcp_phase_sync_t sync,
    float coherence_threshold,
    uint64_t timeout_ms
);

/**
 * @brief Get current phase coherence
 *
 * @param sync Sync handle
 * @return Order parameter r [0,1]
 */
float nimcp_phase_sync_get_coherence(nimcp_phase_sync_t sync);

/**
 * @brief Get mean phase of synchronized group
 *
 * @param sync Sync handle
 * @return Mean phase ψ in radians [0, 2π]
 */
float nimcp_phase_sync_get_mean_phase(nimcp_phase_sync_t sync);

/**
 * @brief Get number of oscillators in sync group
 *
 * @param sync Sync handle
 * @return Number of tracked futures
 */
size_t nimcp_phase_sync_get_count(nimcp_phase_sync_t sync);

/**
 * @brief Destroy phase sync context
 *
 * @param sync Sync handle (NULL safe)
 */
void nimcp_phase_sync_destroy(nimcp_phase_sync_t sync);

//=============================================================================
// Predictive Coding API (Error-driven callbacks)
//=============================================================================

/**
 * @brief Create predictive model for a signal
 *
 * WHAT: Creates Bayesian predictor for named signal
 * WHY:  Callbacks only on prediction errors (efficiency)
 * HOW:  Tracks prediction, precision, updates on observations
 *
 * @param signal_name Unique name for signal
 * @param initial_prediction Initial expected value
 * @param initial_precision Initial certainty (inverse variance)
 * @return Model handle or NULL on failure
 */
nimcp_predictive_model_t nimcp_predictive_create(
    const char* signal_name,
    float initial_prediction,
    float initial_precision
);

/**
 * @brief Register prediction error callback
 *
 * WHAT: Register callback for prediction errors
 * WHY:  Only notified when prediction is wrong
 * HOW:  Callback receives prediction, actual, error, surprise
 *
 * @param model Model handle
 * @param callback Error callback function
 * @param user_data User context
 * @param surprise_threshold Minimum surprise to trigger (0 = always)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_predictive_on_error(
    nimcp_predictive_model_t model,
    nimcp_prediction_error_callback_t callback,
    void* user_data,
    float surprise_threshold
);

/**
 * @brief Update model with observed value
 *
 * WHAT: Provide actual observation to model
 * WHY:  Triggers error callbacks if prediction wrong
 * HOW:  Bayesian update, calculates surprise, fires callbacks
 *
 * @param model Model handle
 * @param actual_value Observed value
 * @return NIMCP_SUCCESS or error code
 *
 * SIDE EFFECTS:
 * - Updates prediction toward actual
 * - Updates precision based on error
 * - Fires callbacks if surprise > threshold
 */
nimcp_error_t nimcp_predictive_observe(
    nimcp_predictive_model_t model,
    float actual_value
);

/**
 * @brief Get current prediction
 *
 * @param model Model handle
 * @return Current predicted value
 */
float nimcp_predictive_get_prediction(nimcp_predictive_model_t model);

/**
 * @brief Get current precision
 *
 * @param model Model handle
 * @return Current precision (inverse variance)
 */
float nimcp_predictive_get_precision(nimcp_predictive_model_t model);

/**
 * @brief Get last surprise value
 *
 * @param model Model handle
 * @return Last calculated surprise (negative log probability)
 */
float nimcp_predictive_get_last_surprise(nimcp_predictive_model_t model);

/**
 * @brief Manually update prediction
 *
 * @param model Model handle
 * @param new_prediction New predicted value
 * @param new_precision New precision (0 = keep current)
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_predictive_set_prediction(
    nimcp_predictive_model_t model,
    float new_prediction,
    float new_precision
);

/**
 * @brief Destroy predictive model
 *
 * @param model Model handle (NULL safe)
 */
void nimcp_predictive_destroy(nimcp_predictive_model_t model);

//=============================================================================
// Glial Signaling API (Calcium wave coordination)
//=============================================================================

/**
 * @brief Initiate calcium wave from source region
 *
 * WHAT: Starts calcium wave propagating through network
 * WHY:  System-wide coordination (slow but global)
 * HOW:  Reaction-diffusion dynamics, wave propagation
 *
 * @param source_region ID of source region
 * @param initial_calcium Initial calcium concentration (μM)
 * @return Wave handle or NULL on failure
 *
 * USE CASES:
 * - Global state transitions
 * - Metabolic resource reallocation
 * - System-wide synchronization signals
 */
nimcp_glial_wave_t nimcp_glial_wave_initiate(
    uint32_t source_region,
    float initial_calcium
);

/**
 * @brief Advance wave propagation
 *
 * WHAT: Steps wave propagation simulation
 * WHY:  Advance calcium diffusion and IP3 dynamics
 * HOW:  Reaction-diffusion equations
 *
 * @param wave Wave handle
 * @param dt_ms Time step (0 = use default)
 * @return NIMCP_SUCCESS or NIMCP_BIO_ERROR_WAVE_EXTINCT
 */
nimcp_error_t nimcp_glial_wave_step(nimcp_glial_wave_t wave, float dt_ms);

/**
 * @brief Get calcium level at region
 *
 * @param wave Wave handle
 * @param region_id Region to query
 * @return Calcium concentration (μM)
 */
float nimcp_glial_wave_get_level_at(nimcp_glial_wave_t wave, uint32_t region_id);

/**
 * @brief Check if wave has reached region
 *
 * @param wave Wave handle
 * @param region_id Region to check
 * @return true if calcium > threshold
 */
bool nimcp_glial_wave_has_reached(nimcp_glial_wave_t wave, uint32_t region_id);

/**
 * @brief Wait for wave to reach region
 *
 * @param wave Wave handle
 * @param region_id Target region
 * @param timeout_ms Timeout
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_glial_wave_wait_for_region(
    nimcp_glial_wave_t wave,
    uint32_t region_id,
    uint64_t timeout_ms
);

/**
 * @brief Register callback for wave arrival
 *
 * @param wave Wave handle
 * @param region_id Region to monitor
 * @param callback Arrival callback
 * @param user_data User context
 * @return NIMCP_SUCCESS or error code
 */
nimcp_error_t nimcp_glial_wave_on_arrival(
    nimcp_glial_wave_t wave,
    uint32_t region_id,
    nimcp_wave_callback_t callback,
    void* user_data
);

/**
 * @brief Get wave propagation radius
 *
 * @param wave Wave handle
 * @return Current radius (μm) from source
 */
float nimcp_glial_wave_get_radius(nimcp_glial_wave_t wave);

/**
 * @brief Check if wave is still active
 *
 * @param wave Wave handle
 * @return true if wave still propagating
 */
bool nimcp_glial_wave_is_active(nimcp_glial_wave_t wave);

/**
 * @brief Destroy glial wave
 *
 * @param wave Wave handle (NULL safe)
 */
void nimcp_glial_wave_destroy(nimcp_glial_wave_t wave);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get channel name as string
 */
static inline const char* nimcp_bio_channel_name(nimcp_bio_channel_type_t channel) {
    switch (channel) {
        case BIO_CHANNEL_DOPAMINE: return "dopamine";
        case BIO_CHANNEL_SEROTONIN: return "serotonin";
        case BIO_CHANNEL_NOREPINEPHRINE: return "norepinephrine";
        case BIO_CHANNEL_ACETYLCHOLINE: return "acetylcholine";
        default: return "unknown";
    }
}

/**
 * @brief Get oscillation band name as string
 */
static inline const char* nimcp_oscillation_band_name(nimcp_oscillation_band_t band) {
    switch (band) {
        case BIO_OSC_DELTA: return "delta";
        case BIO_OSC_THETA: return "theta";
        case BIO_OSC_ALPHA: return "alpha";
        case BIO_OSC_BETA: return "beta";
        case BIO_OSC_GAMMA: return "gamma";
        default: return "unknown";
    }
}

/**
 * @brief Get bio-future state name as string
 */
static inline const char* nimcp_bio_future_state_name(nimcp_bio_future_state_t state) {
    switch (state) {
        case BIO_FUTURE_PENDING: return "pending";
        case BIO_FUTURE_COMPLETED: return "completed";
        case BIO_FUTURE_FAILED: return "failed";
        case BIO_FUTURE_CANCELLED: return "cancelled";
        case BIO_FUTURE_DECAYED: return "decayed";
        case BIO_FUTURE_REFRACTORY: return "refractory";
        default: return "unknown";
    }
}

/**
 * @brief Get decay tau for channel (ms)
 */
static inline float nimcp_bio_channel_decay_tau(nimcp_bio_channel_type_t channel) {
    switch (channel) {
        case BIO_CHANNEL_DOPAMINE: return BIO_COMP_DA_DECAY_TAU_MS;
        case BIO_CHANNEL_SEROTONIN: return BIO_COMP_5HT_DECAY_TAU_MS;
        case BIO_CHANNEL_NOREPINEPHRINE: return BIO_COMP_NE_DECAY_TAU_MS;
        case BIO_CHANNEL_ACETYLCHOLINE: return BIO_COMP_ACH_DECAY_TAU_MS;
        default: return 1000.0f;
    }
}

/**
 * @brief Get center frequency for oscillation band (Hz)
 */
static inline float nimcp_oscillation_center_freq(nimcp_oscillation_band_t band) {
    switch (band) {
        case BIO_OSC_DELTA: return BIO_OSC_DELTA_CENTER_HZ;
        case BIO_OSC_THETA: return BIO_OSC_THETA_CENTER_HZ;
        case BIO_OSC_ALPHA: return BIO_OSC_ALPHA_CENTER_HZ;
        case BIO_OSC_BETA: return BIO_OSC_BETA_CENTER_HZ;
        case BIO_OSC_GAMMA: return BIO_OSC_GAMMA_CENTER_HZ;
        default: return 10.0f;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_BIO_ASYNC_H */
