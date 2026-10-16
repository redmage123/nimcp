/**
 * @file nimcp_rate_coding.h
 * @brief Rate coding: Convert spike trains to/from firing rates
 *
 * WHAT: Bidirectional conversion between spike trains and firing rate features
 * WHY:  Rate coding is fundamental for linking spiking neurons to cognitive features
 * HOW:  Sliding window rate estimation with adaptive binning and temporal smoothing
 *
 * BIOLOGICAL BASIS:
 * - Rate coding is the dominant neural code for many brain regions
 * - Firing rate = spike count / time window
 * - Neurons encode information in average firing rate over 10-1000ms windows
 * - Example: Motor cortex encodes movement direction in firing rate
 *
 * ALGORITHMS:
 * 1. Sliding Window: Count spikes in fixed time windows
 * 2. Bayesian Adaptive Binning: Optimize bin size for rate estimation
 * 3. Exponential Moving Average: Temporal smoothing of rate estimates
 * 4. Burst Detection: Identify and filter burst patterns
 *
 * @author NIMCP Development Team
 * @date 2025-01-19
 */

#ifndef NIMCP_RATE_CODING_H
#define NIMCP_RATE_CODING_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** Maximum number of neurons supported */
#define RATE_CODING_MAX_NEURONS 10000

/** Maximum spike history length */
#define RATE_CODING_MAX_SPIKE_HISTORY 10000

/** Default time window for rate calculation (ms) */
#define RATE_CODING_DEFAULT_WINDOW_MS 100.0f

/** Minimum window size (ms) */
#define RATE_CODING_MIN_WINDOW_MS 1.0f

/** Maximum window size (ms) */
#define RATE_CODING_MAX_WINDOW_MS 1000.0f

//=============================================================================
// Core Types
//=============================================================================

/**
 * WHAT: Spike train representation
 * WHY:  Need compact storage of spike times for rate calculation
 * HOW:  Array of timestamps with count
 */
typedef struct {
    uint64_t* spike_times;  /**< Array of spike timestamps (ms) */
    uint32_t num_spikes;    /**< Number of spikes in array */
    uint32_t capacity;      /**< Allocated capacity */
    uint64_t start_time;    /**< Start time of recording window */
    uint64_t end_time;      /**< End time of recording window */
} spike_train_t;

/**
 * WHAT: Rate coding configuration
 * WHY:  Flexible configuration for different encoding strategies
 * HOW:  Parameters control window size, smoothing, burst detection
 */
typedef struct {
    float window_ms;           /**< Time window for rate calculation (ms) */
    float ema_alpha;           /**< Exponential moving average smoothing factor [0-1] */
    bool enable_burst_filter;  /**< Enable burst detection and filtering */
    float burst_threshold_hz;  /**< Firing rate threshold for burst detection */
    float burst_min_isi_ms;    /**< Minimum inter-spike interval for burst (ms) */
    bool adaptive_binning;     /**< Enable Bayesian adaptive bin size optimization */
} rate_coding_config_t;

/**
 * WHAT: Rate coding encoder/decoder instance
 * WHY:  Maintain state across multiple encode/decode operations
 * HOW:  Opaque handle pattern for encapsulation
 */
typedef struct rate_coding_encoder_struct* rate_coding_encoder_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create rate coding encoder with configuration
 * WHY:  Initialize encoder state and allocate resources
 * HOW:  Validate config, allocate memory, initialize smoothing
 *
 * @param config Encoder configuration (NULL uses defaults)
 * @return Encoder handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (each encoder has independent state)
 */
rate_coding_encoder_t rate_coding_create(const rate_coding_config_t* config);

/**
 * WHAT: Destroy rate coding encoder and free resources
 * WHY:  Clean memory cleanup
 * HOW:  Free all allocated memory
 *
 * @param encoder Encoder to destroy (NULL is safe)
 *
 * COMPLEXITY: O(1)
 */
void rate_coding_destroy(rate_coding_encoder_t encoder);

/**
 * WHAT: Get default rate coding configuration
 * WHY:  Provide sensible defaults for common use case
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - window_ms: 100.0 (standard rate coding window)
 * - ema_alpha: 0.3 (moderate smoothing)
 * - enable_burst_filter: false
 * - burst_threshold_hz: 100.0
 * - burst_min_isi_ms: 5.0
 * - adaptive_binning: true
 */
rate_coding_config_t rate_coding_default_config(void);

//=============================================================================
// Encoding Functions (Spikes → Features)
//=============================================================================

/**
 * WHAT: Encode spike train to firing rate
 * WHY:  Convert temporal spike pattern to rate-based feature
 * HOW:  Count spikes in window, normalize by window duration
 *
 * ALGORITHM:
 * 1. Count spikes in [current_time - window_ms, current_time]
 * 2. Rate = spike_count / (window_ms / 1000.0)  [Hz]
 * 3. Apply exponential moving average if enabled
 * 4. Filter bursts if enabled
 *
 * @param encoder Encoder instance
 * @param spike_train Input spike train
 * @param current_time Current time (ms)
 * @param rate_out Output firing rate (Hz)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = spikes in window
 * RANGE: rate_out in [0, spike_limit_hz] typically [0, 200] Hz
 */
bool rate_coding_encode(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t current_time,
    float* rate_out
);

/**
 * WHAT: Encode multiple spike trains to rate vector
 * WHY:  Efficient batch processing for populations
 * HOW:  Encode each neuron independently, vectorized when possible
 *
 * @param encoder Encoder instance
 * @param spike_trains Array of spike trains (one per neuron)
 * @param num_neurons Number of neurons
 * @param current_time Current time (ms)
 * @param rates_out Output rate vector (size = num_neurons)
 * @return Number of neurons successfully encoded
 *
 * COMPLEXITY: O(n * m) where n = neurons, m = avg spikes per neuron
 */
uint32_t rate_coding_encode_population(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_trains,
    uint32_t num_neurons,
    uint64_t current_time,
    float* rates_out
);

/**
 * WHAT: Encode with multiple time windows simultaneously
 * WHY:  Multi-scale temporal integration (fast and slow dynamics)
 * HOW:  Calculate rates for each window size in parallel
 *
 * @param encoder Encoder instance
 * @param spike_train Input spike train
 * @param current_time Current time (ms)
 * @param windows_ms Array of window sizes (ms)
 * @param num_windows Number of windows
 * @param rates_out Output rates for each window (size = num_windows)
 * @return Number of windows successfully encoded
 *
 * COMPLEXITY: O(s * w) where s = spikes, w = num_windows
 *
 * EXAMPLE:
 * float windows[] = {10.0, 100.0, 1000.0};  // Fast, medium, slow
 * float rates[3];
 * rate_coding_encode_multiscale(enc, train, time, windows, 3, rates);
 * // rates[0] = instantaneous, rates[1] = standard, rates[2] = sustained
 */
uint32_t rate_coding_encode_multiscale(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t current_time,
    const float* windows_ms,
    uint32_t num_windows,
    float* rates_out
);

//=============================================================================
// Decoding Functions (Features → Spikes)
//=============================================================================

/**
 * WHAT: Decode firing rate to spike pattern
 * WHY:  Generate biologically plausible spike train from rate
 * HOW:  Poisson process with given rate, or regular spike train
 *
 * ALGORITHM (Poisson):
 * 1. For each time step dt: P(spike) = rate_hz * dt
 * 2. Generate random number r ~ Uniform(0, 1)
 * 3. If r < P(spike), emit spike at current time
 *
 * ALGORITHM (Regular):
 * 1. Inter-spike interval = 1000.0 / rate_hz  [ms]
 * 2. Place spikes at regular intervals
 *
 * @param encoder Encoder instance
 * @param rate_hz Firing rate to decode (Hz)
 * @param duration_ms Duration of spike train to generate (ms)
 * @param use_poisson true = Poisson process, false = regular spikes
 * @param spike_train_out Output spike train (pre-allocated)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(r * d) where r = rate_hz, d = duration_ms
 */
bool rate_coding_decode(
    rate_coding_encoder_t encoder,
    float rate_hz,
    float duration_ms,
    bool use_poisson,
    spike_train_t* spike_train_out
);

/**
 * WHAT: Decode rate vector to population spike trains
 * WHY:  Generate population activity from feature vector
 * HOW:  Decode each neuron independently
 *
 * @param encoder Encoder instance
 * @param rates_hz Array of firing rates (Hz)
 * @param num_neurons Number of neurons
 * @param duration_ms Duration of spike trains (ms)
 * @param use_poisson Use Poisson spike generation
 * @param spike_trains_out Output spike trains (pre-allocated array)
 * @return Number of neurons successfully decoded
 *
 * COMPLEXITY: O(n * r * d) where n = neurons, r = avg_rate, d = duration
 */
uint32_t rate_coding_decode_population(
    rate_coding_encoder_t encoder,
    const float* rates_hz,
    uint32_t num_neurons,
    float duration_ms,
    bool use_poisson,
    spike_train_t* spike_trains_out
);

//=============================================================================
// Advanced Features
//=============================================================================

/**
 * WHAT: Detect and classify bursts in spike train
 * WHY:  Bursts carry distinct information from tonic firing
 * HOW:  Identify clusters of spikes with short ISI
 *
 * BURST CRITERIA:
 * - ISI < burst_min_isi_ms for consecutive spikes
 * - Firing rate during burst > burst_threshold_hz
 * - Minimum 3 spikes per burst
 *
 * @param encoder Encoder instance
 * @param spike_train Input spike train
 * @param burst_count_out Output: number of bursts detected
 * @param burst_rate_out Output: average rate during bursts (Hz)
 * @param tonic_rate_out Output: average rate outside bursts (Hz)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_spikes
 */
bool rate_coding_detect_bursts(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint32_t* burst_count_out,
    float* burst_rate_out,
    float* tonic_rate_out
);

/**
 * WHAT: Calculate instantaneous firing rate at specific time
 * WHY:  High temporal resolution rate estimation
 * HOW:  Kernel density estimation with Gaussian kernel
 *
 * @param encoder Encoder instance
 * @param spike_train Input spike train
 * @param time_ms Time point for rate estimation
 * @param kernel_width_ms Width of Gaussian kernel (ms)
 * @param inst_rate_out Output instantaneous rate (Hz)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_spikes
 */
bool rate_coding_instantaneous_rate(
    rate_coding_encoder_t encoder,
    const spike_train_t* spike_train,
    uint64_t time_ms,
    float kernel_width_ms,
    float* inst_rate_out
);

//=============================================================================
// Spike Train Utilities
//=============================================================================

/**
 * WHAT: Create spike train with initial capacity
 * WHY:  Initialize spike train structure
 * HOW:  Allocate array and set metadata
 *
 * @param capacity Initial capacity (number of spikes)
 * @return Allocated spike train or NULL on error
 *
 * NOTE: Caller must free with rate_coding_spike_train_destroy()
 */
spike_train_t* rate_coding_spike_train_create(uint32_t capacity);

/**
 * WHAT: Destroy spike train and free memory
 * WHY:  Clean memory cleanup
 * HOW:  Free spike array and structure
 *
 * @param train Spike train to destroy (NULL is safe)
 */
void rate_coding_spike_train_destroy(spike_train_t* train);

/**
 * WHAT: Add spike to spike train
 * WHY:  Build spike train incrementally
 * HOW:  Append to array, resize if needed
 *
 * @param train Spike train
 * @param spike_time Timestamp of spike (ms)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) amortized
 */
bool spike_train_add_spike(spike_train_t* train, uint64_t spike_time);

/**
 * WHAT: Clear all spikes from spike train
 * WHY:  Reuse spike train structure
 * HOW:  Reset count, keep allocated memory
 *
 * @param train Spike train to clear
 */
void rate_coding_spike_train_clear(spike_train_t* train);

/**
 * WHAT: Copy spike train
 * WHY:  Duplicate spike train for independent processing
 * HOW:  Deep copy of all spikes and metadata
 *
 * @param src Source spike train
 * @return Copy of spike train or NULL on error
 */
spike_train_t* spike_train_copy(const spike_train_t* src);

//=============================================================================
// Statistics and Diagnostics
//=============================================================================

/**
 * WHAT: Calculate coefficient of variation of ISI
 * WHY:  Measure spike train regularity
 * HOW:  CV = std(ISI) / mean(ISI)
 *
 * @param spike_train Input spike train
 * @param cv_out Output coefficient of variation
 * @return true on success (needs >= 2 spikes), false otherwise
 *
 * INTERPRETATION:
 * - CV = 0: perfectly regular (clock-like)
 * - CV = 1: Poisson process (random)
 * - CV > 1: bursty, irregular
 */
bool rate_coding_compute_cv(const spike_train_t* spike_train, float* cv_out);

/**
 * WHAT: Calculate Fano factor (variance-to-mean ratio)
 * WHY:  Measure trial-to-trial variability
 * HOW:  Fano = variance(spike_count) / mean(spike_count)
 *
 * @param spike_trains Array of spike trains from repeated trials
 * @param num_trials Number of trials
 * @param window_ms Counting window (ms)
 * @param fano_out Output Fano factor
 * @return true on success, false on error
 *
 * INTERPRETATION:
 * - Fano = 1: Poisson-like variability
 * - Fano < 1: sub-Poisson (regular)
 * - Fano > 1: super-Poisson (bursty)
 */
bool rate_coding_compute_fano_factor(
    const spike_train_t* spike_trains,
    uint32_t num_trials,
    float window_ms,
    float* fano_out
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RATE_CODING_H
