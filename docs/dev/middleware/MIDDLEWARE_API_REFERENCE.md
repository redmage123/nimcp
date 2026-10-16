# NIMCP Middleware API Reference
## Detailed API Specifications and Header Examples

**Version:** 1.0
**Date:** 2025-11-19
**Related:** MIDDLEWARE_ARCHITECTURE.md

---

## Table of Contents

1. [Core Middleware Header](#core-middleware-header)
2. [Rate Coding API](#rate-coding-api)
3. [Feature Extractor API](#feature-extractor-api)
4. [Pattern Detector API](#pattern-detector-api)
5. [Thalamic Router API](#thalamic-router-api)
6. [Event System API](#event-system-api)
7. [Complete Example Headers](#complete-example-headers)

---

## Core Middleware Header

### `include/middleware/nimcp_middleware.h`

**Purpose:** Unified header that includes all middleware components

```c
/**
 * @file nimcp_middleware.h
 * @brief NIMCP Middleware Layer - Bridge between neural infrastructure and cognition
 *
 * WHAT: Comprehensive middleware for spike encoding, feature extraction, pattern
 *       recognition, routing, and normalization
 *
 * WHY:  Decouple low-level neural processing from high-level cognitive systems
 *
 * HOW:  Provides abstractions inspired by biological structures (thalamus,
 *       basal ganglia, association cortex)
 *
 * ARCHITECTURE:
 *   Low-Level (spikes, synapses, neurons)
 *        ↓
 *   Middleware (encoding, features, patterns, routing)
 *        ↓
 *   High-Level (ethics, working memory, reasoning)
 *
 * BIOLOGICAL INSPIRATION:
 *   - Thalamus: Routing and gating (thalamic_router)
 *   - Basal Ganglia: Action selection (pattern_detector)
 *   - Association Cortex: Feature binding (feature_extractor)
 *   - Hippocampus: Pattern separation (sequence_detector)
 *
 * @author NIMCP Development Team
 * @version 1.0
 * @date 2025-11-19
 */

#ifndef NIMCP_MIDDLEWARE_H
#define NIMCP_MIDDLEWARE_H

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Middleware Components
//=============================================================================

// Encoding Layer - Convert spike trains ↔ feature representations
#include "middleware/encoding/nimcp_rate_coding.h"
#include "middleware/encoding/nimcp_temporal_coding.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/encoding/nimcp_spike_patterns.h"

// Feature Extraction - Extract meaningful features from neural populations
#include "middleware/features/nimcp_feature_extractor.h"
#include "middleware/features/nimcp_population_features.h"
#include "middleware/features/nimcp_temporal_features.h"

// Pattern Recognition - Detect patterns in neural activity
#include "middleware/patterns/nimcp_pattern_detector.h"
#include "middleware/patterns/nimcp_sequence_detector.h"
#include "middleware/patterns/nimcp_phase_detector.h"

// Routing - Intelligent signal routing between regions and modules
#include "middleware/routing/nimcp_thalamic_router.h"
#include "middleware/routing/nimcp_attention_gate.h"
#include "middleware/routing/nimcp_region_router.h"

// Buffering - Temporal integration and buffering
#include "middleware/buffers/nimcp_sliding_window.h"
#include "middleware/buffers/nimcp_accumulator.h"
#include "middleware/buffers/nimcp_event_buffer.h"

// Normalization - Signal conditioning for cognitive modules
#include "middleware/normalization/nimcp_signal_normalizer.h"
#include "middleware/normalization/nimcp_adaptive_norm.h"
#include "middleware/normalization/nimcp_homeostatic_norm.h"

// Event System - Asynchronous cognitive communication
#include "middleware/events/nimcp_cognitive_events.h"
#include "middleware/events/nimcp_event_dispatcher.h"
#include "middleware/events/nimcp_event_aggregator.h"

// Integration - Pipeline composition and registry
#include "middleware/integration/nimcp_middleware_pipeline.h"
#include "middleware/integration/nimcp_middleware_registry.h"

//=============================================================================
// Convenience Functions
//=============================================================================

/**
 * @brief Initialize all middleware subsystems
 *
 * WHAT: One-time initialization of middleware infrastructure
 * WHY:  Setup thread pools, allocate shared buffers, register components
 * HOW:  Call once at program start before creating brain
 *
 * @return true on success, false on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Not thread-safe, call before multi-threaded execution
 *
 * EXAMPLE:
 * ```c
 * if (!middleware_init()) {
 *     fprintf(stderr, "Failed to initialize middleware\n");
 *     return -1;
 * }
 *
 * brain_t brain = brain_create("my_brain", BRAIN_SIZE_MEDIUM);
 * // ... use brain ...
 *
 * middleware_shutdown();
 * ```
 */
bool middleware_init(void);

/**
 * @brief Shutdown middleware subsystems and free resources
 *
 * WHAT: Clean shutdown of middleware infrastructure
 * WHY:  Free shared resources, destroy thread pools
 * HOW:  Call once at program end after destroying all brains
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Not thread-safe, call after all middleware usage complete
 */
void middleware_shutdown(void);

/**
 * @brief Get middleware version information
 *
 * @param major Output: major version
 * @param minor Output: minor version
 * @param patch Output: patch version
 */
void middleware_get_version(uint32_t* major, uint32_t* minor, uint32_t* patch);

/**
 * @brief Get middleware build information
 *
 * @return Build string (e.g., "NIMCP Middleware v1.0.0 (2025-11-19)")
 */
const char* middleware_get_build_info(void);

//=============================================================================
// Common Types (Used Across Middleware)
//=============================================================================

/**
 * @brief Feature vector - universal data representation
 *
 * WHAT: Generic feature vector returned by all middleware components
 * WHY:  Unified interface for cognitive modules
 * HOW:  All extractors, detectors, routers return this type
 */
typedef struct {
    float* data;                /**< Feature data (caller must free) */
    uint32_t dim;               /**< Feature dimension */
    uint64_t timestamp_ms;      /**< When features were extracted */
    uint32_t source_region;     /**< Brain region ID (if applicable) */
    char source_name[64];       /**< Human-readable source */
} middleware_feature_vector_t;

/**
 * @brief Free feature vector
 *
 * @param vec Feature vector to free
 */
void middleware_feature_vector_free(middleware_feature_vector_t* vec);

/**
 * @brief Copy feature vector
 *
 * @param src Source vector
 * @param dst Destination vector (allocated by function)
 * @return true on success
 */
bool middleware_feature_vector_copy(
    const middleware_feature_vector_t* src,
    middleware_feature_vector_t* dst
);

/**
 * @brief Middleware statistics
 *
 * WHAT: Global middleware performance statistics
 * WHY:  Monitor overhead, identify bottlenecks
 */
typedef struct {
    uint64_t total_extractions;      /**< Feature extractions performed */
    uint64_t total_detections;       /**< Pattern detections performed */
    uint64_t total_routes;           /**< Routing operations performed */
    uint64_t total_normalizations;   /**< Normalizations performed */
    float avg_extraction_time_us;    /**< Average extraction time (μs) */
    float avg_detection_time_us;     /**< Average detection time (μs) */
    float avg_routing_time_us;       /**< Average routing time (μs) */
    uint64_t total_events_published; /**< Events published */
    uint64_t total_events_processed; /**< Events processed */
    uint32_t active_pipelines;       /**< Active pipeline count */
    uint64_t total_memory_allocated; /**< Total memory (bytes) */
} middleware_stats_t;

/**
 * @brief Get global middleware statistics
 *
 * @param stats Output statistics structure
 * @return true on success
 */
bool middleware_get_stats(middleware_stats_t* stats);

/**
 * @brief Reset middleware statistics
 */
void middleware_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_MIDDLEWARE_H
```

---

## Rate Coding API

### `src/middleware/encoding/nimcp_rate_coding.h`

```c
/**
 * @file nimcp_rate_coding.h
 * @brief Rate-based neural encoding - average firing rate representation
 *
 * WHAT: Convert spike trains ↔ firing rates
 * WHY:  Most common neural code, stable representation of continuous quantities
 * HOW:  Count spikes in time window, optionally smooth with exponential filter
 *
 * BIOLOGICAL BASIS:
 *   - Primary visual cortex (V1): Orientation tuning via firing rate
 *   - Motor cortex (M1): Movement velocity encoded in rate
 *   - Somatosensory cortex: Stimulus intensity via rate code
 *
 * WHEN TO USE:
 *   ✓ Continuous quantities (motor control, value estimation)
 *   ✓ Stable over time (100+ ms windows)
 *   ✓ Integration over population
 *   ✗ Precise timing matters (use temporal coding)
 *   ✗ Rapid transients (< 50 ms)
 *
 * PERFORMANCE:
 *   - Encoding: O(n) where n = spike count
 *   - Decoding: O(t) where t = duration
 *   - Memory: O(1) per neuron
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_RATE_CODING_H
#define NIMCP_RATE_CODING_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuralnet/nimcp_neuralnet.h"  // For spike_record_t

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Rate coding configuration
 */
typedef struct {
    uint32_t window_ms;          /**< Time window for rate calculation (default: 100ms) */
    bool smooth;                 /**< Apply exponential smoothing (default: false) */
    float smoothing_tau_ms;      /**< Smoothing time constant (default: 20ms) */
    bool normalize;              /**< Normalize to max rate (default: false) */
    float max_rate_hz;           /**< Maximum expected rate for normalization (default: 100Hz) */
} rate_coding_config_t;

/**
 * @brief Get default rate coding configuration
 *
 * @return Default configuration (100ms window, no smoothing)
 */
rate_coding_config_t rate_coding_default_config(void);

//=============================================================================
// Rate Coder
//=============================================================================

/**
 * @brief Opaque rate coder handle
 */
typedef struct rate_coder_struct* rate_coder_t;

/**
 * @brief Create rate coder
 *
 * WHAT: Initialize rate encoder/decoder
 * WHY:  Setup parameters for spike → rate conversion
 * HOW:  Allocate internal buffers, configure smoothing
 *
 * @param config Configuration (NULL = use defaults)
 * @return Rate coder handle or NULL on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFETY: Thread-safe creation
 *
 * EXAMPLE:
 * ```c
 * rate_coder_t coder = rate_coder_create(&(rate_coding_config_t){
 *     .window_ms = 50,
 *     .smooth = true,
 *     .smoothing_tau_ms = 10.0f
 * });
 * ```
 */
rate_coder_t rate_coder_create(const rate_coding_config_t* config);

/**
 * @brief Destroy rate coder
 *
 * @param coder Rate coder to destroy
 */
void rate_coder_destroy(rate_coder_t coder);

//=============================================================================
// Encoding (Spikes → Rate)
//=============================================================================

/**
 * @brief Encode single neuron spike train as firing rate
 *
 * WHAT: Convert spike times → average firing rate
 * WHY:  Extract rate-coded information from spike train
 * HOW:  Count spikes in [current_time - window, current_time], divide by window
 *
 * @param coder Rate coder
 * @param spikes Spike times (sorted ascending)
 * @param num_spikes Number of spikes
 * @param current_time_ms Current time (ms)
 * @return Firing rate in Hz
 *
 * COMPLEXITY: O(n) where n = num_spikes
 * THREAD-SAFETY: Not thread-safe per coder instance
 *
 * ALGORITHM:
 * ```
 * rate = (count(spikes in [t - window, t]) / window) * 1000
 * if smooth:
 *     rate = rate_prev * exp(-dt/tau) + rate * (1 - exp(-dt/tau))
 * if normalize:
 *     rate = rate / max_rate
 * ```
 *
 * EXAMPLE:
 * ```c
 * spike_record_t spikes[] = {
 *     {.timestamp = 50, .magnitude = 1.0},
 *     {.timestamp = 75, .magnitude = 1.0},
 *     {.timestamp = 90, .magnitude = 1.0}
 * };
 * float rate = rate_coder_encode(coder, spikes, 3, 100);  // ~30 Hz
 * ```
 */
float rate_coder_encode(
    rate_coder_t coder,
    const spike_record_t* spikes,
    uint32_t num_spikes,
    uint64_t current_time_ms
);

/**
 * @brief Encode population of neurons as rate vector
 *
 * WHAT: Extract firing rates from multiple neurons
 * WHY:  Represent population activity as feature vector
 * HOW:  Apply rate_coder_encode to each neuron
 *
 * @param coder Rate coder
 * @param network Neural network
 * @param neuron_ids Neuron population to encode
 * @param num_neurons Population size
 * @param output_rates Output buffer [num_neurons] (pre-allocated)
 * @return true on success
 *
 * COMPLEXITY: O(n*s) where n = neurons, s = avg spikes per neuron
 * THREAD-SAFETY: Not thread-safe per coder
 *
 * EXAMPLE:
 * ```c
 * uint32_t v1_neurons[128];
 * brain_region_get_neurons(brain, REGION_VISUAL_V1, v1_neurons, 128);
 *
 * float v1_rates[128];
 * rate_coder_encode_population(
 *     coder,
 *     brain->network,
 *     v1_neurons,
 *     128,
 *     v1_rates
 * );
 * // v1_rates now contains firing rate of each V1 neuron
 * ```
 */
bool rate_coder_encode_population(
    rate_coder_t coder,
    const neural_network_t network,
    const uint32_t* neuron_ids,
    uint32_t num_neurons,
    float* output_rates
);

//=============================================================================
// Decoding (Rate → Spikes) - For Testing & Generative Models
//=============================================================================

/**
 * @brief Decode firing rate into Poisson spike train
 *
 * WHAT: Generate spike train from firing rate
 * WHY:  Testing, generative models, synthetic data
 * HOW:  Poisson process with rate parameter
 *
 * @param coder Rate coder
 * @param firing_rate_hz Desired firing rate
 * @param duration_ms Spike train duration
 * @param spikes_out Output spike train (caller must free)
 * @param num_spikes_out Output spike count
 * @return true on success
 *
 * COMPLEXITY: O(rate * duration)
 * THREAD-SAFETY: Thread-safe with different output buffers
 *
 * ALGORITHM:
 * ```
 * dt = 1.0  // 1ms time step
 * p = rate * dt / 1000  // Spike probability per ms
 * for t in [0, duration]:
 *     if rand() < p:
 *         emit spike at time t
 * ```
 *
 * EXAMPLE:
 * ```c
 * spike_record_t* spikes = NULL;
 * uint32_t num_spikes = 0;
 * rate_coder_decode(coder, 50.0f, 1000, &spikes, &num_spikes);
 * // spikes contains ~50 spikes over 1 second
 * free(spikes);
 * ```
 */
bool rate_coder_decode(
    rate_coder_t coder,
    float firing_rate_hz,
    uint64_t duration_ms,
    spike_record_t** spikes_out,
    uint32_t* num_spikes_out
);

//=============================================================================
// Statistics & Utilities
//=============================================================================

/**
 * @brief Rate coder statistics
 */
typedef struct {
    uint64_t total_encodings;     /**< Encoding operations performed */
    uint64_t total_decodings;     /**< Decoding operations performed */
    float avg_rate_hz;            /**< Average encoded rate */
    float min_rate_hz;            /**< Minimum observed rate */
    float max_rate_hz;            /**< Maximum observed rate */
    uint64_t total_spikes_encoded;/**< Total spikes processed */
} rate_coder_stats_t;

/**
 * @brief Get rate coder statistics
 *
 * @param coder Rate coder
 * @param stats Output statistics
 * @return true on success
 */
bool rate_coder_get_stats(rate_coder_t coder, rate_coder_stats_t* stats);

/**
 * @brief Reset rate coder statistics
 *
 * @param coder Rate coder
 */
void rate_coder_reset_stats(rate_coder_t coder);

/**
 * @brief Reset smoothing filter state
 *
 * WHAT: Clear exponential smoothing history
 * WHY:  Start fresh after brain reset or context switch
 *
 * @param coder Rate coder
 */
void rate_coder_reset_filter(rate_coder_t coder);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_RATE_CODING_H
```

---

## Feature Extractor API

### `src/middleware/features/nimcp_feature_extractor.h`

```c
/**
 * @file nimcp_feature_extractor.h
 * @brief Generic feature extraction from neural populations
 *
 * WHAT: Unified API for extracting features from any neural population
 * WHY:  Cognitive modules need feature vectors, not raw spikes
 * HOW:  Abstract over neural dynamics, provide high-level representations
 *
 * SUPPORTED FEATURES:
 *   - FEATURE_FIRING_RATE: Average spike rate
 *   - FEATURE_BURST_RATE: Burst frequency
 *   - FEATURE_SYNCHRONY: Population synchrony index
 *   - FEATURE_OSCILLATION_POWER: Band power (delta, theta, alpha, beta, gamma)
 *   - FEATURE_ENTROPY: Activity entropy (Shannon)
 *   - FEATURE_TEMPORAL_PATTERN: Temporal pattern signature
 *   - FEATURE_SPATIAL_PATTERN: Spatial activity distribution
 *
 * BIOLOGICAL BASIS:
 *   - Association Cortex: Binds features across modalities
 *   - Prefrontal Cortex: Abstract feature representations
 *   - Hippocampus: Pattern separation and completion
 *
 * @author NIMCP Development Team
 * @version 1.0
 */

#ifndef NIMCP_FEATURE_EXTRACTOR_H
#define NIMCP_FEATURE_EXTRACTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "core/neuralnet/nimcp_neuralnet.h"
#include "core/brain/nimcp_brain.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Feature Types
//=============================================================================

/**
 * @brief Feature type enumeration
 */
typedef enum {
    FEATURE_FIRING_RATE,         /**< Average firing rate (Hz) */
    FEATURE_BURST_RATE,          /**< Burst frequency (bursts/sec) */
    FEATURE_SYNCHRONY,           /**< Population synchrony [0-1] */
    FEATURE_OSCILLATION_POWER,   /**< Oscillation band power */
    FEATURE_ENTROPY,             /**< Shannon entropy of activity */
    FEATURE_TEMPORAL_PATTERN,    /**< Temporal autocorrelation */
    FEATURE_SPATIAL_PATTERN,     /**< Spatial activity distribution */
    FEATURE_PHASE_COHERENCE,     /**< Phase locking value */
    FEATURE_SPARSITY,            /**< Activity sparsity [0-1] */
    FEATURE_CUSTOM               /**< User-defined feature */
} feature_type_t;

/**
 * @brief Get feature type name
 *
 * @param type Feature type
 * @return Human-readable name
 */
const char* feature_type_name(feature_type_t type);

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Oscillation band configuration (for FEATURE_OSCILLATION_POWER)
 */
typedef enum {
    BAND_DELTA,   /**< 0.5-4 Hz (deep sleep) */
    BAND_THETA,   /**< 4-8 Hz (memory, navigation) */
    BAND_ALPHA,   /**< 8-13 Hz (relaxed awareness) */
    BAND_BETA,    /**< 13-30 Hz (active thinking) */
    BAND_GAMMA,   /**< 30-100 Hz (binding, attention) */
    BAND_CUSTOM   /**< User-defined band */
} oscillation_band_t;

/**
 * @brief Feature configuration
 */
typedef struct {
    feature_type_t type;         /**< Feature type to extract */
    uint32_t window_ms;          /**< Integration window (default: 100ms) */
    uint32_t step_ms;            /**< Sliding step size (default: 50ms) */

    // Type-specific configuration
    union {
        struct {
            bool smooth;         /**< Apply smoothing */
            float tau_ms;        /**< Smoothing constant */
        } rate_config;

        struct {
            float min_isi_ms;    /**< Min ISI for burst (default: 5ms) */
            uint32_t min_spikes; /**< Min spikes per burst (default: 3) */
        } burst_config;

        struct {
            oscillation_band_t band; /**< Which band to analyze */
            float custom_low_hz;     /**< Custom low frequency */
            float custom_high_hz;    /**< Custom high frequency */
        } oscillation_config;

        struct {
            uint32_t num_bins;   /**< Histogram bins for entropy */
        } entropy_config;

        void* custom_config;     /**< Custom feature config */
    };
} feature_config_t;

/**
 * @brief Get default configuration for feature type
 *
 * @param type Feature type
 * @return Default configuration
 */
feature_config_t feature_config_default(feature_type_t type);

//=============================================================================
// Feature Vector
//=============================================================================

/**
 * @brief Feature vector output
 */
typedef struct {
    float* data;                 /**< Feature data (caller must free) */
    uint32_t dim;                /**< Feature dimension */
    uint64_t timestamp_ms;       /**< When extracted */
    feature_type_t type;         /**< Feature type */
    uint32_t source_region;      /**< Brain region ID */
    char source_name[64];        /**< Human-readable source */
} feature_vector_t;

/**
 * @brief Free feature vector
 *
 * @param vec Feature vector to free
 */
void feature_vector_free(feature_vector_t* vec);

/**
 * @brief Copy feature vector
 *
 * @param src Source
 * @param dst Destination (allocated by function)
 * @return true on success
 */
bool feature_vector_copy(const feature_vector_t* src, feature_vector_t* dst);

//=============================================================================
// Feature Extractor
//=============================================================================

/**
 * @brief Opaque feature extractor handle
 */
typedef struct feature_extractor_struct* feature_extractor_t;

/**
 * @brief Create feature extractor
 *
 * WHAT: Initialize multi-feature extractor
 * WHY:  Extract multiple feature types simultaneously
 * HOW:  Configure extraction pipeline
 *
 * @param configs Array of feature configurations
 * @param num_configs Number of features to extract
 * @return Feature extractor handle or NULL on failure
 *
 * COMPLEXITY: O(num_configs)
 * THREAD-SAFETY: Thread-safe creation
 *
 * EXAMPLE:
 * ```c
 * feature_extractor_t extractor = feature_extractor_create(
 *     (feature_config_t[]){
 *         {.type = FEATURE_FIRING_RATE, .window_ms = 100},
 *         {.type = FEATURE_SYNCHRONY, .window_ms = 50},
 *         {.type = FEATURE_OSCILLATION_POWER, .window_ms = 200,
 *          .oscillation_config = {.band = BAND_THETA}}
 *     },
 *     3  // Extract 3 feature types
 * );
 * ```
 */
feature_extractor_t feature_extractor_create(
    const feature_config_t* configs,
    uint32_t num_configs
);

/**
 * @brief Destroy feature extractor
 *
 * @param extractor Feature extractor to destroy
 */
void feature_extractor_destroy(feature_extractor_t extractor);

//=============================================================================
// Extraction Functions
//=============================================================================

/**
 * @brief Extract features from neural population
 *
 * WHAT: Generic feature extraction from neuron population
 * WHY:  Get high-level representation for cognitive processing
 * HOW:  Apply configured feature extractors to population activity
 *
 * @param extractor Feature extractor
 * @param network Neural network
 * @param neuron_ids Neuron population to extract from
 * @param num_neurons Population size
 * @param current_time_ms Current simulation time
 * @return Feature vector (caller must free with feature_vector_free)
 *
 * COMPLEXITY: O(num_neurons * window_size) for most features
 * THREAD-SAFETY: Not thread-safe per extractor instance
 *
 * OUTPUT DIMENSION:
 *   - Single feature: dim = 1
 *   - Multiple features: dim = num_configs
 *   - Spatial features: dim = num_neurons
 *   - Oscillation power: dim = num_bands
 *
 * EXAMPLE:
 * ```c
 * uint32_t pfc_neurons[256];
 * brain_region_get_neurons(brain, REGION_PREFRONTAL_CORTEX, pfc_neurons, 256);
 *
 * feature_vector_t features = feature_extractor_extract(
 *     extractor,
 *     brain->network,
 *     pfc_neurons,
 *     256,
 *     get_current_time_ms()
 * );
 *
 * // Use features for ethics evaluation
 * action_context_t ctx = {
 *     .features = features.data,
 *     .num_features = features.dim
 * };
 * ethics_evaluation_t eval = ethics_engine_evaluate_action(brain->ethics, &ctx);
 *
 * feature_vector_free(&features);
 * ```
 */
feature_vector_t feature_extractor_extract(
    feature_extractor_t extractor,
    const neural_network_t network,
    const uint32_t* neuron_ids,
    uint32_t num_neurons,
    uint64_t current_time_ms
);

/**
 * @brief Extract features from brain region
 *
 * WHAT: Convenience function for region-based extraction
 * WHY:  Common case: extract from specific brain region
 * HOW:  Get neuron IDs from region, call feature_extractor_extract
 *
 * @param extractor Feature extractor
 * @param brain Brain instance
 * @param region Brain region (REGION_PREFRONTAL_CORTEX, etc.)
 * @return Feature vector (caller must free)
 *
 * EXAMPLE:
 * ```c
 * feature_vector_t hippocampal_features = feature_extractor_extract_region(
 *     extractor,
 *     brain,
 *     REGION_HIPPOCAMPUS_CA3
 * );
 * ```
 */
feature_vector_t feature_extractor_extract_region(
    feature_extractor_t extractor,
    const brain_t brain,
    brain_region_t region
);

/**
 * @brief Extract attended features (attention-weighted)
 *
 * WHAT: Extract features from attention-modulated regions
 * WHY:  Attended information is more salient for cognition
 * HOW:  Weight regions by attention strength before extraction
 *
 * @param extractor Feature extractor
 * @param brain Brain instance
 * @param attention Attention system
 * @return Feature vector (caller must free)
 *
 * BIOLOGICAL BASIS:
 *   - Attended stimuli have enhanced cortical representation
 *   - Attention gates working memory (prefrontal cortex)
 *   - Inattentional blindness: unattended items don't reach awareness
 *
 * EXAMPLE:
 * ```c
 * feature_vector_t attended_features = feature_extractor_extract_attended(
 *     extractor,
 *     brain,
 *     brain->attention
 * );
 *
 * // Add to working memory (attention boost salience)
 * float attention_strength = multihead_attention_get_strength(brain->attention);
 * float salience = base_salience + (attention_strength * 0.3f);
 *
 * working_memory_add(
 *     brain->working_memory,
 *     attended_features.data,
 *     attended_features.dim,
 *     salience
 * );
 * ```
 */
feature_vector_t feature_extractor_extract_attended(
    feature_extractor_t extractor,
    const brain_t brain,
    const multihead_attention_t attention
);

//=============================================================================
// Statistics
//=============================================================================

/**
 * @brief Feature extractor statistics
 */
typedef struct {
    uint64_t total_extractions;   /**< Extractions performed */
    float avg_extraction_time_us; /**< Average time (μs) */
    uint32_t num_feature_types;   /**< Feature types configured */
    uint64_t total_neurons_processed; /**< Neurons processed */
    float avg_feature_dim;        /**< Average output dimension */
} feature_extractor_stats_t;

/**
 * @brief Get feature extractor statistics
 *
 * @param extractor Feature extractor
 * @param stats Output statistics
 * @return true on success
 */
bool feature_extractor_get_stats(
    feature_extractor_t extractor,
    feature_extractor_stats_t* stats
);

/**
 * @brief Reset feature extractor statistics
 *
 * @param extractor Feature extractor
 */
void feature_extractor_reset_stats(feature_extractor_t extractor);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_FEATURE_EXTRACTOR_H
```

---

## Summary

This API reference provides:

1. **Complete Header Examples**: Production-ready header file templates
2. **Detailed Documentation**: Doxygen-style comments for all APIs
3. **Usage Examples**: Concrete code snippets showing how to use each API
4. **Biological Context**: Links to neuroscience for each component
5. **Performance Notes**: Complexity analysis and optimization hints
6. **Type Specifications**: All structs, enums, and function signatures
7. **Thread Safety**: Clear documentation of thread-safety guarantees

**See Also:**
- MIDDLEWARE_ARCHITECTURE.md - High-level design and implementation plan
- NIMCP API documentation - Core brain API reference

---

**End of API Reference**
