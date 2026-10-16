//=============================================================================
// nimcp_pr_pink_noise.h - Prime Resonant Pink Noise Extension
//=============================================================================
/**
 * @file nimcp_pr_pink_noise.h
 * @brief Quaternionic pink noise and fractal timing for Prime Resonant memory
 * @version 1.0.0
 * @date 2026-01-08
 *
 * WHAT: Extends base pink noise with quaternionic correlation and fractal timing
 * WHY:  Prime Resonant memory needs multi-dimensional correlated noise and
 *       1/f-distributed event timing for biologically realistic consolidation
 * HOW:  - Quaternionic pink noise: 4 correlated 1/f streams via Cholesky decomposition
 *       - Fractal timing: 1/f-distributed intervals for memory consolidation events
 *       - COW-enabled buffers: Efficient sharing of noise sequences across memories
 *       - Theta rhythm coupling: Phase-coherent noise for encode/retrieve windows
 *
 * BIOLOGICAL MOTIVATION:
 * ======================
 * 1. QUATERNIONIC DIMENSIONS (w, x, y, z):
 *    - w: Consolidation modulation (hippocampal-cortical transfer)
 *    - x: Emotional valence fluctuation (amygdala input)
 *    - y: Salience fluctuation (attention/arousal)
 *    - z: Accessibility drift (retrieval probability)
 *    Cross-correlations model real brain dynamics:
 *    - w-x: -0.3 (consolidation inversely affects emotional intensity)
 *    - w-y: +0.5 (consolidation increases perceived importance)
 *    - w-z: +0.7 (consolidated memories more accessible)
 *    - x-y: +0.4 (emotional content more salient)
 *    - x-z: +0.2 (emotional memories more accessible)
 *    - y-z: +0.6 (salient memories more accessible)
 *
 * 2. FRACTAL TIMING:
 *    - Memory consolidation events are NOT uniformly distributed
 *    - 1/f distribution of inter-event intervals
 *    - Matches observed hippocampal replay timing (Carr et al., 2011)
 *    - Enables multi-timescale memory dynamics
 *
 * 3. THETA RHYTHM COUPLING:
 *    - Theta (4-8 Hz) modulates memory encoding/retrieval
 *    - Pink noise phase-locked to theta enhances fidelity
 *    - Models CA1-CA3 coordination during memory operations
 *
 * MATHEMATICAL BACKGROUND:
 * ========================
 * Correlated noise generation via Cholesky decomposition:
 *   Given correlation matrix C, find L such that C = L * L^T
 *   Independent noise I = [i_w, i_x, i_y, i_z]
 *   Correlated noise N = L * I
 *   Result: E[N_i * N_j] = C[i][j]
 *
 * Fractal timing via 1/f interval generation:
 *   P(interval = t) ~ 1/t^alpha where alpha approx 1
 *   This produces scale-free timing with long-range correlations
 *
 * PERFORMANCE:
 * ============
 * - Quaternionic sample: O(1) per sample (4 generator calls + matrix mult)
 * - Fractal timing: O(1) per event
 * - COW buffer clone: O(1) via reference counting
 * - Cholesky decomposition: O(1) at init (4x4 matrix)
 *
 * DEPENDENCIES:
 * =============
 * - nimcp_pink_noise.h (base generators)
 * - nimcp_cow_manager.h (COW buffers)
 * - nimcp_memory.h (allocation)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PR_PINK_NOISE_H
#define NIMCP_PR_PINK_NOISE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Configuration Constants
//=============================================================================

/** Number of quaternion components */
#define PR_QUAT_DIM                 4

/** Dimension indices for quaternion components */
#define PR_QUAT_W                   0   /**< Consolidation modulation */
#define PR_QUAT_X                   1   /**< Emotional valence */
#define PR_QUAT_Y                   2   /**< Salience fluctuation */
#define PR_QUAT_Z                   3   /**< Accessibility drift */

/** Default correlation matrix values (biologically-inspired) */
#define PR_CORR_WW                  1.0f    /**< w self-correlation */
#define PR_CORR_WX                 -0.3f    /**< Consolidation-emotion (inverse) */
#define PR_CORR_WY                  0.5f    /**< Consolidation-salience */
#define PR_CORR_WZ                  0.7f    /**< Consolidation-accessibility */
#define PR_CORR_XX                  1.0f    /**< x self-correlation */
#define PR_CORR_XY                  0.4f    /**< Emotion-salience */
#define PR_CORR_XZ                  0.2f    /**< Emotion-accessibility */
#define PR_CORR_YY                  1.0f    /**< y self-correlation */
#define PR_CORR_YZ                  0.6f    /**< Salience-accessibility */
#define PR_CORR_ZZ                  1.0f    /**< z self-correlation */

/** Default theta rhythm parameters */
#define PR_DEFAULT_THETA_FREQ_HZ    6.0f    /**< Theta frequency */
#define PR_DEFAULT_THETA_COUPLING   0.5f    /**< Coupling strength [0-1] */

/** Default fractal timing parameters */
#define PR_DEFAULT_BASE_INTERVAL_MS 100.0f  /**< Base interval between events */
#define PR_FRACTAL_ALPHA            1.0f    /**< 1/f exponent for timing */

/** Default buffer parameters */
#define PR_DEFAULT_BUFFER_SAMPLES   4096    /**< Default buffer size */
#define PR_DEFAULT_SAMPLE_RATE_HZ   1000.0f /**< 1ms resolution */

//=============================================================================
// Forward Declarations
//=============================================================================

/* Opaque handles from base pink noise module */
typedef struct pink_noise_generator_internal_t* pink_noise_generator_t;

/* Opaque handles from COW manager */
typedef struct cow_manager_struct* cow_manager_t;
typedef struct cow_handle_struct* cow_handle_t;

//=============================================================================
// Types and Structures
//=============================================================================

/**
 * @struct pr_quat_sample_t
 * @brief Single quaternionic pink noise sample
 *
 * WHAT: Four correlated noise values representing memory state fluctuations
 * WHY:  Single struct for convenient access to all quaternion components
 * HOW:  Generated from 4 independent streams + correlation transform
 */
typedef struct {
    float w;    /**< Consolidation modulation [-1, +1] */
    float x;    /**< Emotional valence [-1, +1] */
    float y;    /**< Salience fluctuation [-1, +1] */
    float z;    /**< Accessibility drift [-1, +1] */
} pr_quat_sample_t;

/**
 * @struct pr_quat_pink_params_t
 * @brief Configuration for quaternionic pink noise generator
 *
 * WHAT: Parameters controlling noise spectrum and cross-correlations
 * WHY:  Customize noise characteristics for different memory subsystems
 * HOW:  Passed to pr_quat_pink_create()
 */
typedef struct {
    float alpha;            /**< Spectral exponent (1.0 = true pink) */
    float amplitude;        /**< RMS amplitude of each component */
    float sample_rate_hz;   /**< Sampling rate */
    uint32_t seed;          /**< Random seed (0 = time-based) */
} pr_quat_pink_params_t;

/**
 * @struct pr_quat_pink_state_t
 * @brief Quaternionic pink noise generator state
 *
 * WHAT: Maintains 4 correlated 1/f noise streams
 * WHY:  Models multi-dimensional memory state fluctuations
 * HOW:  4 independent generators + Cholesky transform for correlations
 *
 * COMPONENT MEANINGS:
 * - gen_w: Consolidation modulation (hippocampal-cortical transfer rate)
 * - gen_x: Emotional valence fluctuation (amygdala contribution)
 * - gen_y: Salience fluctuation (attentional weighting)
 * - gen_z: Accessibility drift (retrieval probability)
 */
typedef struct {
    /* References to base pink noise generators (from nimcp_pink_noise.h) */
    pink_noise_generator_t gen_w;   /**< Consolidation modulation generator */
    pink_noise_generator_t gen_x;   /**< Emotional valence generator */
    pink_noise_generator_t gen_y;   /**< Salience fluctuation generator */
    pink_noise_generator_t gen_z;   /**< Accessibility drift generator */

    /* Cross-correlation matrix for coherent evolution */
    float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM];

    /* Cholesky decomposition of correlation matrix (lower triangular) */
    float cholesky_L[PR_QUAT_DIM][PR_QUAT_DIM];

    /* Theta rhythm coupling */
    float theta_phase;              /**< Current theta phase [0, 2*pi] */
    float theta_frequency_hz;       /**< Theta oscillation frequency */
    float theta_coupling_strength;  /**< Coupling strength [0, 1] */

    /* Sample rate for phase advancement */
    float sample_rate_hz;

    /* Configuration snapshot */
    pr_quat_pink_params_t params;

    /* Generation statistics */
    uint64_t samples_generated;
} pr_quat_pink_state_t;

/**
 * @struct pr_fractal_timing_t
 * @brief Fractal timing generator for memory consolidation events
 *
 * WHAT: Generates 1/f-distributed inter-event intervals
 * WHY:  Memory consolidation events have scale-free timing in real brains
 * HOW:  Pink noise generator used to produce interval distribution
 *
 * BIOLOGICAL BASIS:
 * - Hippocampal sharp-wave ripples show 1/f timing distribution
 * - Sleep spindles cluster with fractal statistics
 * - Memory replay events have long-range temporal correlations
 */
typedef struct {
    pink_noise_generator_t interval_gen;    /**< Generator for interval noise */
    float base_interval_ms;                 /**< Mean interval */
    float min_interval_ms;                  /**< Minimum interval (prevents clustering) */
    float max_interval_ms;                  /**< Maximum interval (prevents starvation) */
    float last_event_time_ms;               /**< Time of last event */
    uint64_t event_count;                   /**< Total events generated */
    float alpha;                            /**< Spectral exponent for intervals */
} pr_fractal_timing_t;

/**
 * @struct pr_pink_buffer_t
 * @brief COW-enabled pink noise buffer for efficient memory sharing
 *
 * WHAT: Pre-generated pink noise samples with copy-on-write semantics
 * WHY:  Many memories share similar noise modulation during encoding
 * HOW:  COW manager allows O(1) cloning, only copies on modification
 *
 * USE CASES:
 * - Template noise for new memory encoding
 * - Snapshot of noise state at memory creation
 * - Efficient parallel processing of memory batches
 */
typedef struct {
    cow_handle_t noise_handle;      /**< COW handle for noise samples */
    cow_manager_t cow_manager;      /**< COW manager reference (non-owning) */
    size_t sample_count;            /**< Number of samples in buffer */
    size_t current_index;           /**< Current read position */
    float sample_rate_hz;           /**< Sampling rate */
    bool owns_manager;              /**< True if we created the COW manager */
} pr_pink_buffer_t;

/**
 * @struct pr_quat_path_t
 * @brief Smooth quaternion path for gradual state transitions
 *
 * WHAT: Sequence of quaternion samples with controlled smoothness
 * WHY:  Memory state changes should be smooth, not discontinuous
 * HOW:  Interpolated pink noise path through quaternion space
 */
typedef struct {
    pr_quat_sample_t* samples;      /**< Path samples (owned) */
    size_t step_count;              /**< Number of steps in path */
    float smoothness;               /**< Path smoothness factor [0=noisy, 1=smooth] */
    float total_length;             /**< Accumulated path length */
} pr_quat_path_t;

/**
 * @struct pr_pink_noise_stats_t
 * @brief Statistics for pink noise module
 *
 * WHAT: Operational metrics for monitoring and debugging
 * WHY:  Track noise quality and resource usage
 */
typedef struct {
    uint64_t quat_samples_generated;    /**< Total quaternion samples */
    uint64_t fractal_events;            /**< Total fractal timing events */
    uint64_t buffer_clones;             /**< COW buffer clones */
    uint64_t buffer_writes;             /**< COW buffer writes (copies) */
    float mean_interval_ms;             /**< Mean fractal interval */
    float measured_alpha;               /**< Measured spectral exponent */
} pr_pink_noise_stats_t;

//=============================================================================
// Quaternionic Pink Noise API
//=============================================================================

/**
 * @brief Get default quaternionic pink noise parameters
 *
 * WHAT: Returns sensible defaults for memory modulation
 * WHY:  Provides starting point based on neuroscience literature
 * HOW:  Sets alpha=1.0, amplitude=0.05, sample_rate=1000Hz
 *
 * @return Default configuration
 */
NIMCP_EXPORT pr_quat_pink_params_t pr_quat_pink_default_params(void);

/**
 * @brief Get default correlation matrix (biologically-inspired)
 *
 * WHAT: Fills correlation matrix with empirically-derived values
 * WHY:  Cross-correlations model real brain dynamics
 * HOW:  Symmetric positive-definite matrix from neuroscience data
 *
 * DEFAULT VALUES:
 * - w-x: -0.3 (consolidation inversely affects emotional intensity)
 * - w-y: +0.5 (consolidation increases salience)
 * - w-z: +0.7 (consolidated memories more accessible)
 * - x-y: +0.4 (emotional content more salient)
 * - x-z: +0.2 (emotional memories more accessible)
 * - y-z: +0.6 (salient memories more accessible)
 *
 * @param correlation_matrix Output 4x4 correlation matrix
 */
NIMCP_EXPORT void pr_quat_pink_default_correlation(
    float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM]);

/**
 * @brief Create quaternionic pink noise generator
 *
 * WHAT: Initializes 4 correlated pink noise streams
 * WHY:  Enable multi-dimensional memory state modulation
 * HOW:  Creates 4 base generators, computes Cholesky decomposition
 *
 * ALGORITHM:
 *   1. Create 4 independent pink noise generators
 *   2. Validate correlation matrix (positive-definite)
 *   3. Compute Cholesky decomposition: C = L * L^T
 *   4. Store L for runtime correlation transform
 *
 * @param params Generator parameters (NULL for defaults)
 * @param correlation Correlation matrix (NULL for defaults)
 * @return Generator state or NULL on failure
 *
 * COMPLEXITY: O(1) (4x4 Cholesky is constant)
 * MEMORY: ~500 bytes + 4 generator states
 *
 * EXAMPLE:
 * ```c
 * pr_quat_pink_params_t params = pr_quat_pink_default_params();
 * float corr[4][4];
 * pr_quat_pink_default_correlation(corr);
 * pr_quat_pink_state_t* state = pr_quat_pink_create(&params, corr);
 * ```
 */
NIMCP_EXPORT pr_quat_pink_state_t* pr_quat_pink_create(
    const pr_quat_pink_params_t* params,
    const float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM]);

/**
 * @brief Destroy quaternionic pink noise generator
 *
 * WHAT: Frees generator and all resources
 * WHY:  Prevent memory leaks
 * HOW:  Destroys 4 base generators, frees state
 *
 * @param state Generator to destroy (can be NULL)
 */
NIMCP_EXPORT void pr_quat_pink_destroy(pr_quat_pink_state_t* state);

/**
 * @brief Generate next quaternionic pink noise sample
 *
 * WHAT: Produces next correlated 4D noise sample
 * WHY:  Real-time memory state modulation
 * HOW:  Generate 4 independent samples, apply Cholesky transform
 *
 * ALGORITHM:
 *   1. Generate independent samples: I = [i_w, i_x, i_y, i_z]
 *   2. Apply correlation: N = L * I where L is Cholesky factor
 *   3. Apply theta coupling if enabled
 *   4. Return correlated sample
 *
 * @param state Generator state
 * @param sample Output sample
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD SAFETY: Not thread-safe (use per-thread generators)
 *
 * EXAMPLE:
 * ```c
 * pr_quat_sample_t sample;
 * pr_quat_pink_next(state, &sample);
 * memory->quat.w += sample.w * modulation_depth;
 * ```
 */
NIMCP_EXPORT bool pr_quat_pink_next(
    pr_quat_pink_state_t* state,
    pr_quat_sample_t* sample);

/**
 * @brief Generate smooth quaternion path through state space
 *
 * WHAT: Creates interpolated path of quaternion samples
 * WHY:  Smooth state transitions for memory evolution
 * HOW:  Pink noise samples + lowpass filtering for smoothness
 *
 * ALGORITHM:
 *   1. Generate raw quaternion samples at each step
 *   2. Apply exponential smoothing: y[n] = s*x[n] + (1-s)*y[n-1]
 *   3. Higher smoothness = more filtering
 *
 * @param state Generator state
 * @param path Output path structure (caller allocates, we fill)
 * @param steps Number of steps in path
 * @param smoothness Smoothness factor [0=noisy, 1=very smooth]
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(steps)
 * MEMORY: Caller must allocate path->samples
 *
 * EXAMPLE:
 * ```c
 * pr_quat_path_t path;
 * path.samples = nimcp_malloc(100 * sizeof(pr_quat_sample_t));
 * pr_quat_pink_path(state, &path, 100, 0.8f);
 * // Use path for gradual memory state evolution
 * nimcp_free(path.samples);
 * ```
 */
NIMCP_EXPORT bool pr_quat_pink_path(
    pr_quat_pink_state_t* state,
    pr_quat_path_t* path,
    size_t steps,
    float smoothness);

/**
 * @brief Set theta rhythm coupling parameters
 *
 * WHAT: Configures phase-locking to theta oscillation
 * WHY:  Theta rhythm coordinates memory encoding/retrieval
 * HOW:  Modulates noise amplitude based on theta phase
 *
 * COUPLING EFFECT:
 * - At theta peak: noise amplitude enhanced
 * - At theta trough: noise amplitude reduced
 * - Coupling strength controls modulation depth
 *
 * @param state Generator state
 * @param phase Current theta phase [0, 2*pi]
 * @param strength Coupling strength [0=none, 1=full]
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * // During memory encoding (theta peak = pi/2)
 * pr_quat_pink_set_theta_coupling(state, M_PI_2, 0.7f);
 * ```
 */
NIMCP_EXPORT bool pr_quat_pink_set_theta_coupling(
    pr_quat_pink_state_t* state,
    float phase,
    float strength);

/**
 * @brief Advance theta phase by timestep
 *
 * WHAT: Updates internal theta phase based on elapsed time
 * WHY:  Automatic phase tracking without external clock
 * HOW:  phase += 2*pi*frequency*dt
 *
 * @param state Generator state
 * @param dt_ms Time step in milliseconds
 * @return New theta phase
 */
NIMCP_EXPORT float pr_quat_pink_advance_theta(
    pr_quat_pink_state_t* state,
    float dt_ms);

/**
 * @brief Reset quaternionic generator state
 *
 * WHAT: Resets all 4 generators and theta phase
 * WHY:  Restart noise sequence for reproducibility
 * HOW:  Reseeds all base generators
 *
 * @param state Generator state
 * @param new_seed New seed (0 = time-based)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_quat_pink_reset(
    pr_quat_pink_state_t* state,
    uint32_t new_seed);

//=============================================================================
// Fractal Timing API
//=============================================================================

/**
 * @brief Create fractal timing generator
 *
 * WHAT: Initializes 1/f-distributed interval generator
 * WHY:  Memory consolidation events have fractal timing
 * HOW:  Pink noise generator produces interval fluctuations
 *
 * @param base_interval_ms Mean interval between events
 * @return Timing generator or NULL on failure
 *
 * COMPLEXITY: O(1)
 * MEMORY: ~200 bytes
 *
 * EXAMPLE:
 * ```c
 * pr_fractal_timing_t* timing = pr_fractal_timing_create(100.0f);
 * // Events will average 100ms apart with 1/f distribution
 * ```
 */
NIMCP_EXPORT pr_fractal_timing_t* pr_fractal_timing_create(float base_interval_ms);

/**
 * @brief Create fractal timing generator with full parameters
 *
 * WHAT: Initializes 1/f-distributed interval generator with bounds
 * WHY:  Control interval range to prevent extreme values
 * HOW:  Pink noise + clamping to [min, max] range
 *
 * @param base_interval_ms Mean interval between events
 * @param min_interval_ms Minimum interval (prevents clustering)
 * @param max_interval_ms Maximum interval (prevents starvation)
 * @param alpha Spectral exponent (1.0 = true 1/f)
 * @param seed Random seed (0 = time-based)
 * @return Timing generator or NULL on failure
 */
NIMCP_EXPORT pr_fractal_timing_t* pr_fractal_timing_create_ex(
    float base_interval_ms,
    float min_interval_ms,
    float max_interval_ms,
    float alpha,
    uint32_t seed);

/**
 * @brief Destroy fractal timing generator
 *
 * WHAT: Frees generator and resources
 * WHY:  Prevent memory leaks
 *
 * @param timing Generator to destroy (can be NULL)
 */
NIMCP_EXPORT void pr_fractal_timing_destroy(pr_fractal_timing_t* timing);

/**
 * @brief Get next event time with 1/f-distributed interval
 *
 * WHAT: Returns time of next consolidation event
 * WHY:  Schedule memory events with realistic timing
 * HOW:  Current time + pink-noise-modulated interval
 *
 * ALGORITHM:
 *   1. Generate pink noise sample
 *   2. Map to interval: interval = base * (1 + noise)
 *   3. Clamp to [min, max]
 *   4. Return current_time + interval
 *
 * @param timing Timing generator
 * @param current_time_ms Current simulation time
 * @return Next event time in ms
 *
 * COMPLEXITY: O(1)
 *
 * EXAMPLE:
 * ```c
 * float next_consolidation = pr_fractal_next_event_time(timing, sim_time);
 * if (sim_time >= next_consolidation) {
 *     trigger_memory_consolidation();
 *     next_consolidation = pr_fractal_next_event_time(timing, sim_time);
 * }
 * ```
 */
NIMCP_EXPORT float pr_fractal_next_event_time(
    pr_fractal_timing_t* timing,
    float current_time_ms);

/**
 * @brief Check if event is due at current time
 *
 * WHAT: Returns true if consolidation event should occur
 * WHY:  Convenience function for event scheduling
 * HOW:  Compares current time to scheduled event time
 *
 * @param timing Timing generator
 * @param current_time_ms Current simulation time
 * @return true if event is due, false otherwise
 */
NIMCP_EXPORT bool pr_fractal_event_due(
    pr_fractal_timing_t* timing,
    float current_time_ms);

/**
 * @brief Get last event time
 *
 * @param timing Timing generator
 * @return Time of last event in ms
 */
NIMCP_EXPORT float pr_fractal_get_last_event_time(
    const pr_fractal_timing_t* timing);

/**
 * @brief Get event count
 *
 * @param timing Timing generator
 * @return Total number of events generated
 */
NIMCP_EXPORT uint64_t pr_fractal_get_event_count(
    const pr_fractal_timing_t* timing);

/**
 * @brief Reset fractal timing generator
 *
 * WHAT: Resets event count and timing state
 * WHY:  Restart timing sequence
 *
 * @param timing Timing generator
 * @param new_seed New seed (0 = time-based)
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_fractal_timing_reset(
    pr_fractal_timing_t* timing,
    uint32_t new_seed);

//=============================================================================
// COW-Enabled Pink Noise Buffer API
//=============================================================================

/**
 * @brief Create COW-enabled pink noise buffer
 *
 * WHAT: Pre-generates pink noise samples with copy-on-write
 * WHY:  Efficient sharing across memories during batch encoding
 * HOW:  COW manager provides O(1) cloning
 *
 * @param cow_mgr COW manager (NULL to create internal)
 * @param sample_count Number of samples to buffer
 * @param sample_rate_hz Sampling rate
 * @return Buffer or NULL on failure
 *
 * COMPLEXITY: O(sample_count) for initial generation
 * MEMORY: sample_count * sizeof(float) + overhead
 *
 * EXAMPLE:
 * ```c
 * pr_pink_buffer_t* template = pr_pink_buffer_create(NULL, 4096, 1000.0f);
 * pr_pink_buffer_t* clone1 = pr_pink_buffer_clone(template);
 * pr_pink_buffer_t* clone2 = pr_pink_buffer_clone(template);
 * // clone1 and clone2 share memory until written
 * ```
 */
NIMCP_EXPORT pr_pink_buffer_t* pr_pink_buffer_create(
    cow_manager_t cow_mgr,
    size_t sample_count,
    float sample_rate_hz);

/**
 * @brief Create COW-enabled pink noise buffer with parameters
 *
 * WHAT: Pre-generates pink noise with specified spectral characteristics
 * WHY:  Customize noise for different memory types
 *
 * @param cow_mgr COW manager (NULL to create internal)
 * @param sample_count Number of samples
 * @param sample_rate_hz Sampling rate
 * @param alpha Spectral exponent
 * @param amplitude RMS amplitude
 * @param seed Random seed
 * @return Buffer or NULL on failure
 */
NIMCP_EXPORT pr_pink_buffer_t* pr_pink_buffer_create_ex(
    cow_manager_t cow_mgr,
    size_t sample_count,
    float sample_rate_hz,
    float alpha,
    float amplitude,
    uint32_t seed);

/**
 * @brief Clone COW buffer (O(1) operation)
 *
 * WHAT: Creates shared reference to buffer data
 * WHY:  Efficient copying for parallel processing
 * HOW:  COW increments reference count, no data copy
 *
 * @param buffer Source buffer
 * @return Cloned buffer (shares data until written)
 *
 * COMPLEXITY: O(1)
 *
 * NOTE: Clone shares data with original until pr_pink_buffer_write()
 *       is called, at which point actual copy occurs.
 */
NIMCP_EXPORT pr_pink_buffer_t* pr_pink_buffer_clone(
    const pr_pink_buffer_t* buffer);

/**
 * @brief Destroy COW buffer
 *
 * WHAT: Releases buffer resources
 * WHY:  Prevent memory leaks
 * HOW:  Decrements COW reference, frees if last
 *
 * @param buffer Buffer to destroy (can be NULL)
 */
NIMCP_EXPORT void pr_pink_buffer_destroy(pr_pink_buffer_t* buffer);

/**
 * @brief Get next sample from buffer
 *
 * WHAT: Returns next pink noise sample (read-only)
 * WHY:  Streaming access to pre-generated noise
 * HOW:  Circular buffer with wraparound
 *
 * @param buffer Pink noise buffer
 * @param sample Output sample value
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 */
NIMCP_EXPORT bool pr_pink_buffer_next(
    pr_pink_buffer_t* buffer,
    float* sample);

/**
 * @brief Get sample at specific index (read-only)
 *
 * WHAT: Random access to buffer samples
 * WHY:  Non-sequential access patterns
 *
 * @param buffer Pink noise buffer
 * @param index Sample index
 * @param sample Output sample value
 * @return true on success, false if index out of range
 */
NIMCP_EXPORT bool pr_pink_buffer_get(
    const pr_pink_buffer_t* buffer,
    size_t index,
    float* sample);

/**
 * @brief Write sample to buffer (triggers COW if shared)
 *
 * WHAT: Modifies buffer sample, copying if necessary
 * WHY:  Allow modification while preserving sharing benefits
 * HOW:  COW copy on first write if buffer is shared
 *
 * @param buffer Pink noise buffer
 * @param index Sample index
 * @param value New sample value
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1) if already private, O(n) on first write if shared
 */
NIMCP_EXPORT bool pr_pink_buffer_write(
    pr_pink_buffer_t* buffer,
    size_t index,
    float value);

/**
 * @brief Reset buffer read position
 *
 * WHAT: Resets current_index to 0
 * WHY:  Replay buffer from beginning
 *
 * @param buffer Pink noise buffer
 */
NIMCP_EXPORT void pr_pink_buffer_reset_index(pr_pink_buffer_t* buffer);

/**
 * @brief Check if buffer is shared (COW)
 *
 * @param buffer Pink noise buffer
 * @return true if shared with other buffers
 */
NIMCP_EXPORT bool pr_pink_buffer_is_shared(const pr_pink_buffer_t* buffer);

//=============================================================================
// Resonance Modulation API
//=============================================================================

/**
 * @brief Modulate resonance score with pink noise
 *
 * WHAT: Applies 1/f fluctuation to memory resonance score
 * WHY:  Real memory retrieval has stochastic variability
 * HOW:  resonance_out = base_resonance * (1 + depth * noise)
 *
 * @param buffer Pink noise buffer
 * @param base_resonance Base resonance score [0, 1]
 * @param depth Modulation depth [0, 1]
 * @param resonance_out Output modulated resonance
 * @return true on success, false on failure
 *
 * EXAMPLE:
 * ```c
 * float modulated_resonance;
 * pr_pink_modulate_resonance(buffer, 0.8f, 0.1f, &modulated_resonance);
 * // modulated_resonance will be in [0.72, 0.88] with 1/f variability
 * ```
 */
NIMCP_EXPORT bool pr_pink_modulate_resonance(
    pr_pink_buffer_t* buffer,
    float base_resonance,
    float depth,
    float* resonance_out);

/**
 * @brief Modulate quaternion state with correlated pink noise
 *
 * WHAT: Applies quaternionic noise to memory quaternion
 * WHY:  Memory states evolve with correlated fluctuations
 * HOW:  quat += depth * quat_noise (component-wise)
 *
 * @param quat_state Quaternionic noise generator
 * @param base_quat Input quaternion [w, x, y, z]
 * @param depth Modulation depth [0, 1]
 * @param out_quat Output modulated quaternion
 * @return true on success, false on failure
 */
NIMCP_EXPORT bool pr_pink_modulate_quaternion(
    pr_quat_pink_state_t* quat_state,
    const float base_quat[PR_QUAT_DIM],
    float depth,
    float out_quat[PR_QUAT_DIM]);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Validate correlation matrix (positive-definite check)
 *
 * WHAT: Checks if correlation matrix is valid for Cholesky decomposition
 * WHY:  Invalid matrices cause numerical instability
 * HOW:  Attempt Cholesky decomposition, return success status
 *
 * @param correlation_matrix 4x4 correlation matrix
 * @return true if valid (positive-definite), false otherwise
 */
NIMCP_EXPORT bool pr_quat_pink_validate_correlation(
    const float correlation_matrix[PR_QUAT_DIM][PR_QUAT_DIM]);

/**
 * @brief Get pink noise module statistics
 *
 * WHAT: Returns operational metrics
 * WHY:  Monitoring and debugging
 *
 * @param stats Output statistics structure
 */
NIMCP_EXPORT void pr_pink_noise_get_stats(pr_pink_noise_stats_t* stats);

/**
 * @brief Reset pink noise module statistics
 */
NIMCP_EXPORT void pr_pink_noise_reset_stats(void);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_pink_noise_get_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_PR_PINK_NOISE_H */
