//=============================================================================
// nimcp_pr_snn_bridge.h - SNN Bridge for Prime Resonant Memory System
//=============================================================================
/**
 * @file nimcp_pr_snn_bridge.h
 * @brief Integration bridge between PR Memory System and Spiking Neural Networks
 *
 * WHAT: Bidirectional bridge enabling quaternion state <-> spike pattern encoding
 * WHY:  Enable biologically realistic memory representation through spike-based
 *       neural computation, leveraging temporal coding for semantic states
 * HOW:  Maps quaternion components to different spike coding schemes (rate,
 *       burst, population, latency) and decodes spike statistics back to
 *       quaternion state
 *
 * NEUROSCIENCE FOUNDATION:
 *
 *   Quaternion -> Spike Encoding:
 *   +-----------------------------------------------------------------------+
 *   |  Each quaternion component maps to a different spike coding scheme:  |
 *   |                                                                       |
 *   |  w (consolidation) -> Rate Coding: Higher w = higher firing rate     |
 *   |    Biological basis: Strongly consolidated memories elicit more      |
 *   |    robust, sustained neural activity in cortical representations     |
 *   |                                                                       |
 *   |  x (emotion)       -> Burst Coding: Valence determines burst pattern |
 *   |    Positive: Regular bursts (4-6 spikes, 5ms ISI)                    |
 *   |    Negative: Irregular bursts (2-3 spikes, 20ms ISI)                 |
 *   |    Biological basis: Amygdala neurons show distinct burst patterns   |
 *   |    for positive vs negative emotional valence                        |
 *   |                                                                       |
 *   |  y (salience)      -> Population Coding: Recruit more neurons        |
 *   |    High salience = larger population response                        |
 *   |    Biological basis: Dopaminergic salience signal recruits attention |
 *   |    networks with proportional population activation                  |
 *   |                                                                       |
 *   |  z (accessibility) -> Latency Coding: Higher z = shorter latency     |
 *   |    Easily accessible = fast first-spike response                     |
 *   |    Biological basis: Hippocampal pattern completion speed reflects   |
 *   |    memory accessibility (tip-of-tongue phenomenon)                   |
 *   +-----------------------------------------------------------------------+
 *
 *   Spike -> Quaternion Decoding:
 *   +-----------------------------------------------------------------------+
 *   |  Spike statistics are decoded back to semantic state:                |
 *   |                                                                       |
 *   |  Mean firing rate -> w (consolidation strength estimate)             |
 *   |    Higher sustained rate indicates stronger consolidation            |
 *   |                                                                       |
 *   |  Burst index      -> x (emotional signature)                         |
 *   |    Regular bursts -> positive valence                                |
 *   |    Irregular bursts -> negative valence                              |
 *   |    No bursts -> neutral valence                                      |
 *   |                                                                       |
 *   |  Active neurons   -> y (salience from population response)           |
 *   |    Fraction of population activated indicates salience level         |
 *   |                                                                       |
 *   |  First spike time -> z (accessibility from response latency)         |
 *   |    Faster first spike = higher accessibility                         |
 *   +-----------------------------------------------------------------------+
 *
 *   Entanglement via Spike Correlation:
 *   +-----------------------------------------------------------------------+
 *   |  Memory entanglement is encoded as spike train correlation:          |
 *   |                                                                       |
 *   |  Cross-correlation coefficient between spike trains indicates        |
 *   |  entanglement strength. STDP-like mechanisms strengthen edges        |
 *   |  when spike patterns co-occur within plasticity windows.             |
 *   |                                                                       |
 *   |  Biological basis: Hebbian learning - neurons that fire together     |
 *   |  wire together. Spike-timing dependent plasticity in hippocampus.    |
 *   +-----------------------------------------------------------------------+
 *
 * PERFORMANCE:
 * - Quaternion to spike encoding: ~50us per quaternion
 * - Spike to quaternion decoding: ~30us per pattern
 * - Memory node encoding: ~100us (includes signature encoding)
 * - Pattern operations: ~10us per merge/copy
 *
 * THREAD SAFETY:
 * - Bridge maintains internal mutex for configuration and statistics
 * - Encoding/decoding operations are thread-safe
 * - Pattern operations require external synchronization if shared
 *
 * INTEGRATION:
 * - Core: nimcp_quaternion.h for quaternion mathematics
 * - Core: nimcp_pr_memory_node.h for memory nodes
 * - Core: nimcp_entanglement.h for entanglement edges
 * - SNN: nimcp_snn.h for spike encoding/decoding
 * - SNN: nimcp_snn_encoding.h for coding schemes
 *
 * @author NIMCP Development Team
 * @date 2026-01-09
 * @version 1.0.0
 */

#ifndef NIMCP_PR_SNN_BRIDGE_H
#define NIMCP_PR_SNN_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "snn/nimcp_snn.h"
#include "snn/nimcp_snn_encoding.h"
#include "cognitive/memory/core/nimcp_quaternion.h"
#include "cognitive/memory/core/nimcp_pr_memory_node.h"
#include "cognitive/memory/core/nimcp_entanglement.h"
#include "cognitive/memory/core/nimcp_resonance.h"
#include "constants/nimcp_math_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

// Export macro (for shared library builds)
#ifndef NIMCP_EXPORT
    #define NIMCP_EXPORT
#endif

//=============================================================================
// Constants
//=============================================================================

/** Default population size per quaternion component */
#define PR_SNN_DEFAULT_POPULATION_SIZE      64

/** Default simulation timestep in milliseconds */
#define PR_SNN_DEFAULT_DT_MS                0.1f

/** Default maximum firing rate in Hz */
#define PR_SNN_DEFAULT_MAX_RATE_HZ          100.0f

/** Default minimum spike latency in ms */
#define PR_SNN_DEFAULT_MIN_LATENCY_MS       1.0f

/** Default maximum spike latency in ms */
#define PR_SNN_DEFAULT_MAX_LATENCY_MS       50.0f

/** Default burst threshold (ISI below this = burst) in ms */
#define PR_SNN_DEFAULT_BURST_THRESHOLD_MS   10.0f

/** Default encoding window duration in ms */
#define PR_SNN_DEFAULT_ENCODING_WINDOW_MS   100.0f

/** Maximum spikes per pattern */
#define PR_SNN_MAX_SPIKES_PER_PATTERN       65536

/** Maximum neurons per population */
#define PR_SNN_MAX_NEURONS_PER_POP          1024

/** Spike time indicating no spike */
#define PR_SNN_NO_SPIKE                     (-1.0f)

/** Default noise level for biological realism */
#define PR_SNN_DEFAULT_NOISE_LEVEL          0.05f

/** STDP window for entanglement strengthening (ms) */
#define PR_SNN_STDP_WINDOW_MS               20.0f

/** STDP maximum weight change */
#define PR_SNN_STDP_MAX_DELTA               0.1f

/** Epsilon for floating-point comparisons */
#define PR_SNN_EPSILON                      1e-6f

/** Pi constant */

/** Two Pi */
#ifndef NIMCP_TWO_PI_F
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

/* Forward declaration for SNN network (from nimcp_snn_types.h) */
typedef struct snn_network_s snn_network_t;

//=============================================================================
// Enumerations
//=============================================================================

/**
 * @brief Encoding scheme for each quaternion component
 *
 * WHAT: Specifies the spike coding scheme for encoding values
 * WHY:  Different quaternion components map to different neural coding strategies
 * HOW:  Each scheme uses different temporal/population characteristics
 *
 * Neural Coding Reference:
 * - Rate: Adrian (1928) - Firing rate encodes stimulus intensity
 * - Burst: Krahe & Gabbiani (2004) - Burst patterns encode features
 * - Population: Georgopoulos (1986) - Population vector encoding
 * - Latency: Thorpe (2001) - Time-to-first-spike for fast processing
 * - Phase: O'Keefe & Recce (1993) - Phase precession in hippocampus
 * - Rank Order: Thorpe (1990) - Order of first spikes encodes info
 */
typedef enum {
    PR_SNN_ENCODE_RATE = 0,       /**< Rate coding (for consolidation/w) */
    PR_SNN_ENCODE_BURST,          /**< Burst coding (for emotion/x) */
    PR_SNN_ENCODE_POPULATION,     /**< Population coding (for salience/y) */
    PR_SNN_ENCODE_LATENCY,        /**< Latency/temporal coding (for accessibility/z) */
    PR_SNN_ENCODE_PHASE,          /**< Phase coding (optional, for theta binding) */
    PR_SNN_ENCODE_RANK_ORDER,     /**< Rank order coding (optional, for rapid coding) */
    PR_SNN_ENCODE_COUNT           /**< Number of encoding types */
} pr_snn_encoding_t;

/**
 * @brief Error codes for SNN bridge operations
 */
typedef enum {
    PR_SNN_SUCCESS = 0,               /**< Operation succeeded */
    PR_SNN_ERROR_NULL_POINTER = -1,   /**< NULL pointer argument */
    PR_SNN_ERROR_INVALID_CONFIG = -2, /**< Invalid configuration */
    PR_SNN_ERROR_NO_MEMORY = -3,      /**< Memory allocation failed */
    PR_SNN_ERROR_INVALID_STATE = -4,  /**< Invalid bridge state */
    PR_SNN_ERROR_INVALID_PATTERN = -5,/**< Invalid spike pattern */
    PR_SNN_ERROR_ENCODING_FAILED = -6,/**< Encoding operation failed */
    PR_SNN_ERROR_DECODING_FAILED = -7,/**< Decoding operation failed */
    PR_SNN_ERROR_SNN_FAILED = -8,     /**< SNN operation failed */
    PR_SNN_ERROR_NODE_FAILED = -9,    /**< Memory node operation failed */
    PR_SNN_ERROR_ENTANGLE_FAILED = -10, /**< Entanglement operation failed */
    PR_SNN_ERROR_PATTERN_FULL = -11,  /**< Pattern buffer full */
    PR_SNN_ERROR_INVALID_ENCODING = -12 /**< Invalid encoding type */
} pr_snn_error_t;

//=============================================================================
// Data Structures
//=============================================================================

/**
 * @brief Spike pattern structure
 *
 * WHAT: Container for a spike train from one or more neurons
 * WHY:  Encapsulates all information about a spike pattern for processing
 * HOW:  Stores spike times with associated neuron IDs
 *
 * Memory layout: ~16 bytes header + variable spike data
 * - With 1000 spikes: ~16 + 1000*(4+4) = 8016 bytes
 */
typedef struct {
    float* spike_times;           /**< Array of spike times (ms) */
    uint32_t* neuron_ids;         /**< Which neuron spiked (indices) */
    size_t num_spikes;            /**< Total spike count */
    size_t capacity;              /**< Allocated capacity */
    float duration_ms;            /**< Pattern duration (time window) */
    size_t num_neurons;           /**< Population size (neurons in pattern) */
} pr_spike_pattern_t;

/**
 * @brief Decoded quaternion with confidence metrics
 *
 * WHAT: Result of spike-to-quaternion decoding
 * WHY:  Includes confidence and error for quality assessment
 * HOW:  Quaternion plus statistical measures
 */
typedef struct {
    nimcp_quaternion_t quat;      /**< Decoded quaternion state */
    float confidence;             /**< Decoding confidence [0, 1] */
    float reconstruction_error;   /**< Error in reconstruction [0, 1] */
    float w_confidence;           /**< Per-component confidence: w */
    float x_confidence;           /**< Per-component confidence: x */
    float y_confidence;           /**< Per-component confidence: y */
    float z_confidence;           /**< Per-component confidence: z */
} pr_decoded_quat_t;

/**
 * @brief Bridge configuration
 *
 * WHAT: Parameters controlling encoding/decoding behavior
 * WHY:  Different applications need different spike coding parameters
 * HOW:  Set at creation time, some modifiable afterward
 *
 * Default Configuration:
 * - population_size: 64 neurons per component
 * - simulation_dt_ms: 0.1 ms timestep
 * - max_rate_hz: 100 Hz maximum firing rate
 * - min_latency_ms: 1 ms minimum latency
 * - max_latency_ms: 50 ms maximum latency
 * - burst_threshold: 10 ms ISI for burst detection
 * - encoding_window_ms: 100 ms encoding duration
 * - enable_noise: true (biological realism)
 * - noise_level: 0.05 (5% noise)
 */
typedef struct {
    size_t population_size;       /**< Neurons per quaternion component */
    float simulation_dt_ms;       /**< Simulation timestep (ms) */
    float max_rate_hz;            /**< Maximum firing rate (Hz) */
    float min_latency_ms;         /**< Minimum spike latency (ms) */
    float max_latency_ms;         /**< Maximum spike latency (ms) */
    float burst_threshold_ms;     /**< ISI threshold for bursts (ms) */
    float encoding_window_ms;     /**< Duration of encoding window (ms) */
    bool enable_noise;            /**< Add biological noise */
    float noise_level;            /**< Noise amplitude [0, 1] */
    bool enable_phase_coding;     /**< Enable phase-locked encoding */
    float theta_frequency_hz;     /**< Theta oscillation frequency (Hz) */
    bool track_statistics;        /**< Enable statistics tracking */
} pr_snn_bridge_config_t;

/**
 * @brief Burst pattern parameters
 *
 * WHAT: Configuration for burst coding
 * WHY:  Different emotional valences produce different burst patterns
 * HOW:  Parameters control burst structure for positive/negative emotions
 */
typedef struct {
    uint32_t positive_spikes;     /**< Spikes per positive burst (4-6) */
    float positive_isi_ms;        /**< ISI for positive bursts (5 ms) */
    uint32_t negative_spikes;     /**< Spikes per negative burst (2-3) */
    float negative_isi_ms;        /**< ISI for negative bursts (20 ms) */
    float burst_probability;      /**< Base probability of bursting */
} pr_snn_burst_params_t;

/**
 * @brief Encoding statistics for a single operation
 *
 * WHAT: Detailed statistics from one encoding/decoding operation
 * WHY:  Enable analysis and debugging of encoding quality
 */
typedef struct {
    float input_value;            /**< Input value that was encoded */
    pr_snn_encoding_t encoding;   /**< Encoding scheme used */
    size_t spikes_generated;      /**< Number of spikes generated */
    float encoding_time_us;       /**< Time taken to encode (microseconds) */
    float first_spike_time_ms;    /**< Time of first spike */
    float mean_firing_rate_hz;    /**< Resulting firing rate */
    float burst_index;            /**< Burst index (for burst coding) */
} pr_snn_encode_stats_t;

/**
 * @brief Bridge statistics
 *
 * WHAT: Aggregate operational metrics for the bridge
 * WHY:  Monitor bridge health and performance over time
 */
typedef struct {
    uint64_t total_encodings;         /**< Total quaternion->spike encodings */
    uint64_t total_decodings;         /**< Total spike->quaternion decodings */
    uint64_t total_node_encodings;    /**< Total memory node encodings */
    uint64_t total_entangle_ops;      /**< Total entanglement operations */
    double avg_reconstruction_error;  /**< Running average reconstruction error */
    double avg_spike_count;           /**< Average spikes per encoding */
    double avg_encoding_time_us;      /**< Average encoding time (us) */
    double avg_decoding_time_us;      /**< Average decoding time (us) */
    uint64_t pattern_allocations;     /**< Total pattern allocations */
    uint64_t pattern_deallocations;   /**< Total pattern deallocations */
    size_t peak_pattern_size;         /**< Peak spike count in any pattern */
    uint64_t last_reset_time_ms;      /**< When stats were last reset */
} pr_snn_bridge_stats_t;

/**
 * @brief SNN Bridge handle (opaque)
 *
 * The bridge maintains:
 * - Configuration parameters
 * - Working buffers for encoding/decoding
 * - Statistics counters
 * - Internal mutex for thread safety
 * - RNG state for noise generation
 */
typedef struct pr_snn_bridge_struct* pr_snn_bridge_t;

/**
 * @brief Component-wise encoded patterns
 *
 * WHAT: Separate spike patterns for each quaternion component
 * WHY:  Enable individual analysis of each semantic dimension
 * HOW:  Four patterns (w, x, y, z) plus combined pattern
 */
typedef struct {
    pr_spike_pattern_t* w_pattern;  /**< Consolidation encoding (rate) */
    pr_spike_pattern_t* x_pattern;  /**< Emotion encoding (burst) */
    pr_spike_pattern_t* y_pattern;  /**< Salience encoding (population) */
    pr_spike_pattern_t* z_pattern;  /**< Accessibility encoding (latency) */
    pr_spike_pattern_t* combined;   /**< All components merged */
} pr_snn_component_patterns_t;

/**
 * @brief Inter-spike interval statistics
 *
 * WHAT: Statistics about inter-spike intervals in a pattern
 * WHY:  ISI analysis for burst detection and rate estimation
 */
typedef struct {
    float mean_isi_ms;            /**< Mean inter-spike interval */
    float std_isi_ms;             /**< Standard deviation of ISI */
    float min_isi_ms;             /**< Minimum ISI observed */
    float max_isi_ms;             /**< Maximum ISI observed */
    float cv_isi;                 /**< Coefficient of variation (std/mean) */
    size_t isi_count;             /**< Number of ISIs computed */
} pr_snn_isi_stats_t;

/**
 * @brief Entanglement spike correlation
 *
 * WHAT: Correlation between two spike patterns for entanglement
 * WHY:  Measure of how closely two memory patterns relate
 * HOW:  Cross-correlation coefficient and STDP-relevant timing
 */
typedef struct {
    float correlation;            /**< Cross-correlation coefficient [-1, 1] */
    float synchrony;              /**< Spike synchrony measure [0, 1] */
    float mean_delay_ms;          /**< Mean delay between patterns */
    size_t coincident_spikes;     /**< Spikes within STDP window */
    float stdp_weight_change;     /**< Suggested weight change from STDP */
} pr_snn_spike_correlation_t;

//=============================================================================
// Configuration Functions
//=============================================================================

/**
 * @brief Get default bridge configuration
 *
 * WHAT: Returns sensible default configuration
 * WHY:  Provides starting point for most use cases
 * HOW:  Sets biologically realistic parameters
 *
 * @return Default configuration with:
 *         - population_size: 64
 *         - simulation_dt_ms: 0.1
 *         - max_rate_hz: 100.0
 *         - min_latency_ms: 1.0
 *         - max_latency_ms: 50.0
 *         - burst_threshold_ms: 10.0
 *         - encoding_window_ms: 100.0
 *         - enable_noise: true
 *         - noise_level: 0.05
 *         - enable_phase_coding: false
 *         - track_statistics: true
 *
 * Performance: ~5ns
 *
 * Example:
 *   pr_snn_bridge_config_t config = pr_snn_bridge_config_default();
 *   config.population_size = 128;  // Double population for more precision
 *   pr_snn_bridge_t bridge = pr_snn_bridge_create(&config);
 */
NIMCP_EXPORT pr_snn_bridge_config_t pr_snn_bridge_config_default(void);

/**
 * @brief Validate bridge configuration
 *
 * WHAT: Checks configuration values are valid
 * WHY:  Prevent invalid configs causing runtime errors
 *
 * @param config Configuration to validate
 * @return true if valid, false otherwise
 *
 * Validation rules:
 * - population_size must be > 0 and <= PR_SNN_MAX_NEURONS_PER_POP
 * - simulation_dt_ms must be > 0
 * - max_rate_hz must be > 0 and reasonable (< 1000 Hz)
 * - min_latency_ms must be >= 0 and < max_latency_ms
 * - noise_level must be in [0, 1]
 *
 * Performance: ~10ns
 */
NIMCP_EXPORT bool pr_snn_bridge_config_validate(const pr_snn_bridge_config_t* config);

/**
 * @brief Get default burst parameters
 *
 * @return Default burst pattern configuration
 */
NIMCP_EXPORT pr_snn_burst_params_t pr_snn_burst_params_default(void);

//=============================================================================
// Bridge Lifecycle Functions
//=============================================================================

/**
 * @brief Create SNN bridge with configuration
 *
 * WHAT: Creates a new SNN bridge instance
 * WHY:  Central entry point for PR<->SNN integration
 * HOW:  Allocates bridge, initializes encoders/decoders, sets up buffers
 *
 * @param config Bridge configuration (NULL for defaults)
 * @return New bridge handle, or NULL on error
 *
 * Performance: ~50us (allocation + initialization)
 * Memory: ~10KB base + population_size * 4 components * 8 bytes
 *
 * Example:
 *   pr_snn_bridge_config_t config = pr_snn_bridge_config_default();
 *   pr_snn_bridge_t bridge = pr_snn_bridge_create(&config);
 *   if (!bridge) {
 *       fprintf(stderr, "Failed: %s\n", pr_snn_get_last_error());
 *   }
 */
NIMCP_EXPORT pr_snn_bridge_t pr_snn_bridge_create(const pr_snn_bridge_config_t* config);

/**
 * @brief Destroy SNN bridge
 *
 * WHAT: Frees all bridge resources
 * WHY:  Clean shutdown and resource release
 * HOW:  Frees encoders, decoders, buffers, mutex
 *
 * @param bridge Bridge handle (NULL safe)
 *
 * Performance: ~10us
 */
NIMCP_EXPORT void pr_snn_bridge_destroy(pr_snn_bridge_t bridge);

/**
 * @brief Reset bridge statistics
 *
 * WHAT: Clears all statistical counters
 * WHY:  Start fresh measurement period
 * HOW:  Zeros all stats, updates reset timestamp
 *
 * @param bridge Bridge handle
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~1us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_reset(pr_snn_bridge_t bridge);

/**
 * @brief Get current bridge configuration
 *
 * @param bridge Bridge handle
 * @param config Output configuration (caller-allocated)
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_config(
    const pr_snn_bridge_t bridge,
    pr_snn_bridge_config_t* config
);

/**
 * @brief Update bridge configuration
 *
 * NOTE: Not all parameters can be changed after creation.
 *       population_size cannot be changed.
 *
 * @param bridge Bridge handle
 * @param config New configuration
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_set_config(
    pr_snn_bridge_t bridge,
    const pr_snn_bridge_config_t* config
);

//=============================================================================
// Quaternion -> Spikes Encoding Functions
//=============================================================================

/**
 * @brief Encode full quaternion to spike pattern
 *
 * WHAT: Converts quaternion state to complete spike pattern
 * WHY:  Primary encoding function for memory state representation
 * HOW:  Encodes each component with appropriate scheme, merges results
 *
 * Encoding scheme per component:
 * - w (consolidation): Rate coding - higher w = higher firing rate
 * - x (emotion): Burst coding - valence determines burst pattern
 * - y (salience): Population coding - salience recruits more neurons
 * - z (accessibility): Latency coding - higher z = shorter latency
 *
 * @param bridge Bridge handle
 * @param quat Quaternion state to encode
 * @param pattern Output spike pattern (must be pre-allocated via pr_spike_pattern_create)
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~50us
 *
 * Example:
 *   nimcp_quaternion_t q = quat_create(0.8f, 0.3f, 0.7f, 0.9f);
 *   pr_spike_pattern_t* pattern = pr_spike_pattern_create(256, 100.0f);
 *   if (pr_snn_encode_quaternion(bridge, q, pattern) == PR_SNN_SUCCESS) {
 *       printf("Generated %zu spikes\n", pattern->num_spikes);
 *   }
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_quaternion(
    pr_snn_bridge_t bridge,
    nimcp_quaternion_t quat,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode quaternion with component-wise output
 *
 * WHAT: Encodes quaternion and returns separate patterns per component
 * WHY:  Enable individual analysis of each semantic dimension
 * HOW:  Same as encode_quaternion but keeps patterns separate
 *
 * @param bridge Bridge handle
 * @param quat Quaternion state to encode
 * @param patterns Output component patterns (must be pre-allocated)
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~60us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_quaternion_components(
    pr_snn_bridge_t bridge,
    nimcp_quaternion_t quat,
    pr_snn_component_patterns_t* patterns
);

/**
 * @brief Encode single component with specified scheme
 *
 * WHAT: Encodes a single scalar value using specified encoding
 * WHY:  Low-level function for custom encoding strategies
 * HOW:  Applies specified encoding scheme to value
 *
 * @param bridge Bridge handle
 * @param value Value to encode (typically [0, 1] or [-1, 1])
 * @param encoding Encoding scheme to use
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~10us
 *
 * Example:
 *   // Encode salience using population coding
 *   pr_spike_pattern_t* pop_pattern = pr_spike_pattern_create(64, 100.0f);
 *   pr_snn_encode_component(bridge, 0.8f, PR_SNN_ENCODE_POPULATION, pop_pattern);
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_component(
    pr_snn_bridge_t bridge,
    float value,
    pr_snn_encoding_t encoding,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode value using rate coding
 *
 * WHAT: Generates spike pattern with firing rate proportional to value
 * WHY:  Maps consolidation strength to neural firing rate
 * HOW:  rate = value * max_rate; generates Poisson spikes at this rate
 *
 * @param bridge Bridge handle
 * @param value Value to encode [0, 1]
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~8us
 *
 * Formula: rate = value * max_rate_hz
 * Value 0.0 -> 0 Hz (no spikes)
 * Value 1.0 -> max_rate_hz (e.g., 100 Hz)
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_rate(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode value using burst coding
 *
 * WHAT: Generates burst patterns based on emotional valence
 * WHY:  Maps emotional valence to distinct temporal patterns
 * HOW:  Positive -> regular bursts; Negative -> irregular bursts
 *
 * @param bridge Bridge handle
 * @param value Emotional valence [-1, +1]
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~12us
 *
 * Patterns:
 * - Positive (value > 0): 4-6 spikes with 5ms ISI
 * - Negative (value < 0): 2-3 spikes with 20ms ISI
 * - Neutral (value ~= 0): Sparse, no bursts
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_burst(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode value using population coding
 *
 * WHAT: Recruits fraction of population proportional to value
 * WHY:  Maps salience to population-wide activation level
 * HOW:  active_neurons = value * population_size
 *
 * @param bridge Bridge handle
 * @param value Salience [0, 1]
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~10us
 *
 * Formula: active = round(value * population_size)
 * Value 0.0 -> 0 neurons active
 * Value 1.0 -> all neurons active
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_population(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode value using latency coding
 *
 * WHAT: Encodes value as time-to-first-spike
 * WHY:  Maps accessibility to response latency
 * HOW:  latency = max_latency - value * (max_latency - min_latency)
 *
 * @param bridge Bridge handle
 * @param value Accessibility [0, 1]
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~6us
 *
 * Formula: latency = max_latency - value * (max_latency - min_latency)
 * Value 0.0 -> max_latency (slow, hard to access)
 * Value 1.0 -> min_latency (fast, easy to access)
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_latency(
    pr_snn_bridge_t bridge,
    float value,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Encode value using phase coding
 *
 * WHAT: Encodes value as spike phase relative to theta oscillation
 * WHY:  Leverages theta phase for temporal binding
 * HOW:  phase = value * 2*pi (spike at this phase of theta cycle)
 *
 * @param bridge Bridge handle
 * @param value Value [0, 1] mapping to phase [0, 2*pi]
 * @param theta_phase Current theta phase [0, 2*pi]
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~8us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_phase(
    pr_snn_bridge_t bridge,
    float value,
    float theta_phase,
    pr_spike_pattern_t* pattern
);

//=============================================================================
// Spikes -> Quaternion Decoding Functions
//=============================================================================

/**
 * @brief Decode spike pattern to quaternion
 *
 * WHAT: Reconstructs quaternion state from spike pattern
 * WHY:  Primary decoding function for retrieving memory state
 * HOW:  Extracts rate, burst, population, latency stats; maps to (w,x,y,z)
 *
 * Decoding scheme:
 * - Mean firing rate -> w (consolidation)
 * - Burst index -> x (emotion)
 * - Active neuron fraction -> y (salience)
 * - Inverse first-spike latency -> z (accessibility)
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern
 * @param decoded Output decoded quaternion with confidence
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~30us
 *
 * Example:
 *   pr_decoded_quat_t decoded;
 *   if (pr_snn_decode_to_quaternion(bridge, pattern, &decoded) == PR_SNN_SUCCESS) {
 *       printf("Decoded: (%.2f, %.2f, %.2f, %.2f) conf=%.2f\n",
 *              decoded.quat.w, decoded.quat.x, decoded.quat.y, decoded.quat.z,
 *              decoded.confidence);
 *   }
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_decode_to_quaternion(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern,
    pr_decoded_quat_t* decoded
);

/**
 * @brief Decode spike pattern to quaternion from component patterns
 *
 * WHAT: Reconstructs quaternion from separate component patterns
 * WHY:  Higher accuracy when component patterns are available
 * HOW:  Decodes each component separately, combines
 *
 * @param bridge Bridge handle
 * @param patterns Component patterns structure
 * @param decoded Output decoded quaternion with confidence
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~40us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_decode_from_components(
    pr_snn_bridge_t bridge,
    const pr_snn_component_patterns_t* patterns,
    pr_decoded_quat_t* decoded
);

/**
 * @brief Decode rate from spike pattern
 *
 * WHAT: Extracts firing rate and maps to value
 * WHY:  Decode consolidation strength from rate-coded pattern
 * HOW:  rate = spike_count / duration; value = rate / max_rate
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern
 * @return Decoded value [0, 1], or -1.0f on error
 *
 * Performance: ~5us
 */
NIMCP_EXPORT float pr_snn_decode_rate(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Decode burst pattern to emotional valence
 *
 * WHAT: Analyzes burst characteristics to determine valence
 * WHY:  Decode emotional content from burst-coded pattern
 * HOW:  Compute burst index; regular bursts -> positive, irregular -> negative
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern
 * @return Decoded valence [-1, +1], or 0 on error
 *
 * Performance: ~10us
 */
NIMCP_EXPORT float pr_snn_decode_burst(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Decode population activity to salience
 *
 * WHAT: Measures active neuron fraction
 * WHY:  Decode salience from population-coded pattern
 * HOW:  value = unique_active_neurons / population_size
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern
 * @return Decoded salience [0, 1], or -1.0f on error
 *
 * Performance: ~8us
 */
NIMCP_EXPORT float pr_snn_decode_population(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Decode latency to accessibility
 *
 * WHAT: Measures first-spike latency
 * WHY:  Decode accessibility from latency-coded pattern
 * HOW:  value = (max_latency - first_spike) / (max_latency - min_latency)
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern
 * @return Decoded accessibility [0, 1], or -1.0f on error
 *
 * Performance: ~4us
 */
NIMCP_EXPORT float pr_snn_decode_latency(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern
);

//=============================================================================
// Memory Node Integration Functions
//=============================================================================

/**
 * @brief Encode full memory node to spike pattern
 *
 * WHAT: Encodes complete memory node including signature and state
 * WHY:  Full memory representation as spike pattern for SNN processing
 * HOW:  Encodes quaternion state + signature characteristics
 *
 * @param bridge Bridge handle
 * @param node Memory node to encode
 * @param pattern Output spike pattern
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~100us
 *
 * Example:
 *   pr_spike_pattern_t* pattern = pr_spike_pattern_create(512, 100.0f);
 *   if (pr_snn_encode_node(bridge, memory_node, pattern) == PR_SNN_SUCCESS) {
 *       // Use pattern for SNN-based retrieval
 *   }
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_node(
    pr_snn_bridge_t bridge,
    const pr_memory_node_t* node,
    pr_spike_pattern_t* pattern
);

/**
 * @brief Update memory node from spike pattern
 *
 * WHAT: Decodes spike pattern and updates node's quaternion state
 * WHY:  Allow SNN processing to modify memory state
 * HOW:  Decodes pattern to quaternion, updates node state
 *
 * @param bridge Bridge handle
 * @param pattern Input spike pattern (from SNN processing)
 * @param node Memory node to update (modified in place)
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~50us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_decode_to_node(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern,
    pr_memory_node_t* node
);

/**
 * @brief Perform SNN-based memory retrieval
 *
 * WHAT: Uses SNN to find memories matching a query pattern
 * WHY:  Leverage SNN for pattern completion and associative retrieval
 * HOW:  Encodes query, runs through SNN, decodes top-K results
 *
 * @param bridge Bridge handle
 * @param query_signature Query content signature
 * @param snn SNN network for processing
 * @param top_k Number of top matches to return
 * @param result_ids Output array for matching node IDs
 * @param result_scores Output array for match scores
 * @return Number of matches found, or -1 on error
 *
 * Performance: ~500us (depends on SNN size)
 */
NIMCP_EXPORT int pr_snn_retrieve_via_snn(
    pr_snn_bridge_t bridge,
    const prime_signature_t* query_signature,
    snn_network_t* snn,
    size_t top_k,
    uint64_t* result_ids,
    float* result_scores
);

//=============================================================================
// Entanglement Integration Functions
//=============================================================================

/**
 * @brief Encode entanglement edge as spike correlation
 *
 * WHAT: Represents entanglement as correlated spike patterns
 * WHY:  Enable SNN to process and strengthen memory associations
 * HOW:  Generates correlated patterns for linked memories
 *
 * @param bridge Bridge handle
 * @param edge Entanglement edge to encode
 * @param pattern1 Output pattern for from_node
 * @param pattern2 Output pattern for to_node
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~80us
 *
 * The correlation between pattern1 and pattern2 reflects the
 * resonance_score of the edge. Stronger edges produce more
 * synchronized spike patterns.
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_encode_entanglement(
    pr_snn_bridge_t bridge,
    const entangle_edge_t* edge,
    pr_spike_pattern_t* pattern1,
    pr_spike_pattern_t* pattern2
);

/**
 * @brief Decode spike correlation to entanglement strength
 *
 * WHAT: Measures correlation between two patterns
 * WHY:  Determine entanglement strength from spike timing
 * HOW:  Computes cross-correlation, synchrony, STDP weight change
 *
 * @param bridge Bridge handle
 * @param pattern1 First spike pattern
 * @param pattern2 Second spike pattern
 * @param correlation Output correlation statistics
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~40us
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_decode_entanglement(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2,
    pr_snn_spike_correlation_t* correlation
);

/**
 * @brief Strengthen entanglement edge via STDP-like mechanism
 *
 * WHAT: Applies spike-timing dependent weight change to edge
 * WHY:  Enable Hebbian learning for memory associations
 * HOW:  Computes STDP weight change from spike timing, updates edge
 *
 * @param bridge Bridge handle
 * @param edge Entanglement edge to strengthen (modified in place)
 * @param pattern1 Spike pattern from from_node
 * @param pattern2 Spike pattern from to_node
 * @return Weight change applied, or NaN on error
 *
 * Performance: ~30us
 *
 * STDP Rule:
 * - Pre before post: Potentiation (strengthen)
 * - Post before pre: Depression (weaken)
 * - Change magnitude depends on spike timing within STDP window
 */
NIMCP_EXPORT float pr_snn_strengthen_via_spikes(
    pr_snn_bridge_t bridge,
    entangle_edge_t* edge,
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2
);

//=============================================================================
// Pattern Operations
//=============================================================================

/**
 * @brief Create empty spike pattern
 *
 * WHAT: Allocates spike pattern with specified capacity
 * WHY:  Prepare container for encoding results
 * HOW:  Allocates spike_times and neuron_ids arrays
 *
 * @param num_neurons Population size
 * @param duration_ms Pattern duration in milliseconds
 * @return New pattern, or NULL on error
 *
 * Performance: ~1us per 100 neurons
 * Memory: ~8 bytes per potential spike (preallocated based on expected rate)
 *
 * Example:
 *   pr_spike_pattern_t* pattern = pr_spike_pattern_create(64, 100.0f);
 *   // Use pattern...
 *   pr_spike_pattern_destroy(pattern);
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_create(
    size_t num_neurons,
    float duration_ms
);

/**
 * @brief Create spike pattern with specific capacity
 *
 * @param num_neurons Population size
 * @param duration_ms Pattern duration
 * @param capacity Maximum spikes to preallocate
 * @return New pattern, or NULL on error
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_create_with_capacity(
    size_t num_neurons,
    float duration_ms,
    size_t capacity
);

/**
 * @brief Destroy spike pattern
 *
 * WHAT: Frees all pattern memory
 * WHY:  Resource cleanup
 * HOW:  Frees arrays, then pattern struct
 *
 * @param pattern Pattern to destroy (NULL safe)
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT void pr_spike_pattern_destroy(pr_spike_pattern_t* pattern);

/**
 * @brief Copy spike pattern
 *
 * WHAT: Creates deep copy of pattern
 * WHY:  Allow independent modification of patterns
 * HOW:  Allocates new arrays, copies all data
 *
 * @param pattern Pattern to copy
 * @return New copy, or NULL on error
 *
 * Performance: ~500ns per 1000 spikes
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_copy(
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Clear spike pattern for reuse
 *
 * WHAT: Removes all spikes but keeps allocation
 * WHY:  Efficient reuse without reallocation
 * HOW:  Sets num_spikes to 0
 *
 * @param pattern Pattern to clear
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_clear(pr_spike_pattern_t* pattern);

/**
 * @brief Add spike to pattern
 *
 * WHAT: Appends single spike to pattern
 * WHY:  Build patterns incrementally
 * HOW:  Adds to arrays, grows if necessary
 *
 * @param pattern Pattern to modify
 * @param time_ms Spike time in milliseconds
 * @param neuron_id Neuron that spiked
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~50ns (amortized)
 */
NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_add_spike(
    pr_spike_pattern_t* pattern,
    float time_ms,
    uint32_t neuron_id
);

/**
 * @brief Merge multiple patterns into one
 *
 * WHAT: Combines multiple spike patterns
 * WHY:  Aggregate component patterns into unified pattern
 * HOW:  Concatenates all spikes, sorts by time
 *
 * @param patterns Array of patterns to merge
 * @param count Number of patterns
 * @return New merged pattern, or NULL on error
 *
 * Performance: ~1us per 100 total spikes
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_merge(
    const pr_spike_pattern_t* const* patterns,
    size_t count
);

/**
 * @brief Sort pattern spikes by time
 *
 * @param pattern Pattern to sort
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_spike_pattern_sort(pr_spike_pattern_t* pattern);

/**
 * @brief Extract pattern for specific neuron range
 *
 * @param pattern Source pattern
 * @param neuron_start First neuron ID to include
 * @param neuron_end Last neuron ID to include (exclusive)
 * @return New pattern with only specified neurons
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_extract_neurons(
    const pr_spike_pattern_t* pattern,
    uint32_t neuron_start,
    uint32_t neuron_end
);

/**
 * @brief Extract pattern for specific time range
 *
 * @param pattern Source pattern
 * @param time_start Start time (ms)
 * @param time_end End time (ms)
 * @return New pattern with only spikes in time range
 */
NIMCP_EXPORT pr_spike_pattern_t* pr_spike_pattern_extract_time(
    const pr_spike_pattern_t* pattern,
    float time_start,
    float time_end
);

//=============================================================================
// Spike Statistics Functions
//=============================================================================

/**
 * @brief Compute mean firing rate from pattern
 *
 * WHAT: Calculates average firing rate across population
 * WHY:  Core statistic for rate-based decoding
 * HOW:  rate = num_spikes / (duration * num_neurons)
 *
 * @param pattern Spike pattern
 * @return Mean firing rate in Hz, or -1.0f on error
 *
 * Performance: ~100ns
 */
NIMCP_EXPORT float pr_spike_compute_rate(const pr_spike_pattern_t* pattern);

/**
 * @brief Compute per-neuron firing rates
 *
 * @param pattern Spike pattern
 * @param rates Output array (size = num_neurons, caller-allocated)
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~1us per 1000 spikes
 */
NIMCP_EXPORT pr_snn_error_t pr_spike_compute_neuron_rates(
    const pr_spike_pattern_t* pattern,
    float* rates
);

/**
 * @brief Compute burst index from pattern
 *
 * WHAT: Measures burstiness of spike pattern
 * WHY:  Core statistic for emotion decoding
 * HOW:  burst_index based on ISI distribution relative to threshold
 *
 * @param bridge Bridge handle (for burst threshold)
 * @param pattern Spike pattern
 * @return Burst index [-1, +1]: positive = regular bursts, negative = irregular
 *
 * Performance: ~5us per 1000 spikes
 *
 * Burst index interpretation:
 * - +1: Perfect regular bursting (positive emotion)
 * - 0: Random/Poisson spiking (neutral)
 * - -1: Highly irregular bursting (negative emotion)
 */
NIMCP_EXPORT float pr_spike_compute_burst_index(
    pr_snn_bridge_t bridge,
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Count active neurons in pattern
 *
 * WHAT: Counts unique neurons that fired
 * WHY:  Core statistic for population decoding
 * HOW:  Count unique neuron_ids
 *
 * @param pattern Spike pattern
 * @return Number of active neurons, or 0 on error
 *
 * Performance: ~2us per 1000 spikes
 */
NIMCP_EXPORT size_t pr_spike_compute_population_size(
    const pr_spike_pattern_t* pattern
);

/**
 * @brief Compute mean first-spike latency
 *
 * WHAT: Average time to first spike across neurons
 * WHY:  Core statistic for latency decoding
 * HOW:  For each neuron, find first spike; average across neurons
 *
 * @param pattern Spike pattern
 * @return Mean first-spike latency in ms, or -1.0f on error
 *
 * Performance: ~3us per 1000 spikes
 */
NIMCP_EXPORT float pr_spike_compute_latency(const pr_spike_pattern_t* pattern);

/**
 * @brief Compute inter-spike interval statistics
 *
 * WHAT: Detailed ISI analysis
 * WHY:  Comprehensive burst and regularity analysis
 * HOW:  Compute all ISIs, calculate statistics
 *
 * @param pattern Spike pattern
 * @param stats Output ISI statistics
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~5us per 1000 spikes
 */
NIMCP_EXPORT pr_snn_error_t pr_spike_compute_isi(
    const pr_spike_pattern_t* pattern,
    pr_snn_isi_stats_t* stats
);

/**
 * @brief Compute synchrony measure
 *
 * WHAT: Measures spike synchronization across neurons
 * WHY:  Important for binding and coherence analysis
 * HOW:  Based on spike time coincidences
 *
 * @param pattern Spike pattern
 * @param window_ms Synchrony window (default: 5ms)
 * @return Synchrony [0, 1], or -1.0f on error
 *
 * Performance: ~10us per 1000 spikes
 */
NIMCP_EXPORT float pr_spike_compute_synchrony(
    const pr_spike_pattern_t* pattern,
    float window_ms
);

/**
 * @brief Compute coefficient of variation of ISI
 *
 * WHAT: CV = std(ISI) / mean(ISI)
 * WHY:  Indicates regularity: CV < 1 regular, CV > 1 irregular
 *
 * @param pattern Spike pattern
 * @return CV value, or -1.0f on error
 */
NIMCP_EXPORT float pr_spike_compute_cv(const pr_spike_pattern_t* pattern);

//=============================================================================
// Synchronization Functions
//=============================================================================

/**
 * @brief Compute cross-pattern coherence
 *
 * WHAT: Measures coherence between multiple patterns
 * WHY:  Assess binding strength across memory representations
 * HOW:  Pairwise correlation, returns mean coherence
 *
 * @param patterns Array of patterns
 * @param count Number of patterns
 * @return Mean coherence [0, 1], or -1.0f on error
 *
 * Performance: O(count^2 * spikes)
 */
NIMCP_EXPORT float pr_snn_sync_coherence(
    const pr_spike_pattern_t* const* patterns,
    size_t count
);

/**
 * @brief Phase-lock pattern to theta oscillation
 *
 * WHAT: Shifts spike times to align with theta phase
 * WHY:  Enable theta-gamma coupling for memory binding
 * HOW:  Modulates spike times based on theta phase
 *
 * @param bridge Bridge handle
 * @param pattern Pattern to phase-lock (modified in place)
 * @param theta_phase Target theta phase [0, 2*pi]
 * @return PR_SNN_SUCCESS or error code
 *
 * Performance: ~2us per 100 spikes
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_phase_lock(
    pr_snn_bridge_t bridge,
    pr_spike_pattern_t* pattern,
    float theta_phase
);

/**
 * @brief Compute phase-locking value between patterns
 *
 * @param pattern1 First pattern
 * @param pattern2 Second pattern
 * @param frequency Reference frequency (Hz)
 * @return PLV [0, 1], or -1.0f on error
 */
NIMCP_EXPORT float pr_snn_compute_plv(
    const pr_spike_pattern_t* pattern1,
    const pr_spike_pattern_t* pattern2,
    float frequency
);

//=============================================================================
// Statistics Functions
//=============================================================================

/**
 * @brief Get bridge statistics
 *
 * @param bridge Bridge handle
 * @param stats Output statistics structure
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_stats(
    const pr_snn_bridge_t bridge,
    pr_snn_bridge_stats_t* stats
);

/**
 * @brief Get last encoding statistics
 *
 * @param bridge Bridge handle
 * @param stats Output encode statistics
 * @return PR_SNN_SUCCESS or error code
 */
NIMCP_EXPORT pr_snn_error_t pr_snn_bridge_get_last_encode_stats(
    const pr_snn_bridge_t bridge,
    pr_snn_encode_stats_t* stats
);

/**
 * @brief Print pattern summary to stdout (debug)
 *
 * @param pattern Pattern to summarize
 */
NIMCP_EXPORT void pr_spike_pattern_print(const pr_spike_pattern_t* pattern);

/**
 * @brief Print bridge state summary (debug)
 *
 * @param bridge Bridge handle
 */
NIMCP_EXPORT void pr_snn_bridge_print_state(const pr_snn_bridge_t bridge);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * @brief Get error string for error code
 *
 * @param error Error code
 * @return Human-readable error string
 */
NIMCP_EXPORT const char* pr_snn_error_string(pr_snn_error_t error);

/**
 * @brief Get last error message
 *
 * @return Error string or NULL if no error
 */
NIMCP_EXPORT const char* pr_snn_get_last_error(void);

/**
 * @brief Get encoding name as string
 *
 * @param encoding Encoding type
 * @return Static string name (e.g., "RATE", "BURST")
 */
NIMCP_EXPORT const char* pr_snn_encoding_name(pr_snn_encoding_t encoding);

/**
 * @brief Create component patterns structure
 *
 * @param num_neurons Neurons per component
 * @param duration_ms Pattern duration
 * @return New component patterns, or NULL on error
 */
NIMCP_EXPORT pr_snn_component_patterns_t* pr_snn_component_patterns_create(
    size_t num_neurons,
    float duration_ms
);

/**
 * @brief Destroy component patterns structure
 *
 * @param patterns Component patterns to destroy (NULL safe)
 */
NIMCP_EXPORT void pr_snn_component_patterns_destroy(
    pr_snn_component_patterns_t* patterns
);

/**
 * @brief Get current time in microseconds
 *
 * @return Microseconds since epoch (for timing measurements)
 */
NIMCP_EXPORT uint64_t pr_snn_current_time_us(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PR_SNN_BRIDGE_H
