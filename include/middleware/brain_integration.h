//=============================================================================
// brain_integration.h - Middleware Integration with Brain System
//=============================================================================

#ifndef NIMCP_BRAIN_INTEGRATION_H
#define NIMCP_BRAIN_INTEGRATION_H

#include "middleware/nimcp_middleware.h"
#include "middleware/encoding/nimcp_population_coding.h"
#include "middleware/features/nimcp_feature_extractor.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file brain_integration.h
 * @brief Helper functions for integrating middleware with brain system
 *
 * WHAT: Convenient wrappers for using middleware in brain/cognitive modules
 * WHY:  Simplify middleware usage in NIMCP neural systems
 * HOW:  High-level APIs that combine buffering and normalization
 *
 * USAGE EXAMPLE:
 * ```c
 * // In cognitive module (e.g., working_memory, consolidation)
 *
 * // 1. Create buffering system for neural activity
 * brain_temporal_buffer_t* buffer = brain_create_temporal_buffer(
 *     num_neurons,
 *     BUFFER_SIZE_100MS
 * );
 *
 * // 2. Create normalizer for extracted features
 * brain_feature_normalizer_t* normalizer = brain_create_feature_normalizer(
 *     num_features,
 *     NORMALIZE_ZSCORE
 * );
 *
 * // 3. Use in update loop
 * void working_memory_update(working_memory_t* wm, float dt) {
 *     // Buffer neural activity
 *     brain_buffer_activity(wm->buffer, wm->activity, wm->num_neurons, dt);
 *
 *     // Extract features from buffered data
 *     float features[10];
 *     brain_extract_windowed_features(wm->buffer, features, 10);
 *
 *     // Normalize features
 *     brain_normalize_features(wm->normalizer, features, 10);
 *
 *     // Use normalized features for memory operations
 *     // ...
 * }
 * ```
 */

//=============================================================================
// BUFFER SIZE PRESETS
//=============================================================================

typedef enum {
    BUFFER_SIZE_10MS = 0,    /**< Fast timescale (10ms window, ~100 samples @10kHz) */
    BUFFER_SIZE_100MS = 1,   /**< Medium timescale (100ms window, ~1k samples) */
    BUFFER_SIZE_1S = 2,      /**< Slow timescale (1s window, ~10k samples) */
    BUFFER_SIZE_CUSTOM = 3   /**< Custom size specified by user */
} brain_buffer_size_t;

//=============================================================================
// NORMALIZATION PRESETS
//=============================================================================

typedef enum {
    NORMALIZE_ZSCORE = 0,      /**< Z-score normalization (mean=0, std=1) */
    NORMALIZE_MINMAX = 1,      /**< Min-max scaling to [0, 1] */
    NORMALIZE_ADAPTIVE = 2,    /**< Adaptive normalization (learning rate adapts) */
    NORMALIZE_HOMEOSTATIC = 3, /**< Homeostatic regulation (activity-dependent) */
    NORMALIZE_NONE = 4         /**< No normalization */
} brain_normalize_type_t;

//=============================================================================
// INTEGRATION STRUCTURES
//=============================================================================

/**
 * @brief Temporal buffer manager for neural signals
 *
 * WHAT: Manages multi-timescale buffering of neural activity
 * WHY:  Cognitive modules need temporal context for decision-making
 * HOW:  Combines sliding windows and integration buffers
 */
typedef struct {
    sliding_window_t* window;          /**< Main sliding window */
    integration_buffer_t* multiscale;  /**< Multi-timescale integration */
    temporal_accumulator_t* accumulator; /**< Temporal integration */
    size_t num_channels;               /**< Number of neural channels */
    brain_buffer_size_t size_preset;   /**< Buffer size configuration */
} brain_temporal_buffer_t;

/**
 * @brief Feature normalizer for extracted neural features
 *
 * WHAT: Manages normalization of extracted features
 * WHY:  Stable learning requires normalized inputs
 * HOW:  Applies appropriate normalization based on feature type
 */
typedef struct {
    zscore_normalizer_t* zscore;       /**< Z-score normalizer */
    min_max_normalizer_t* minmax;      /**< Min-max normalizer */
    adaptive_normalizer_t* adaptive;   /**< Adaptive normalizer */
    homeostatic_normalizer_t* homeo;   /**< Homeostatic normalizer */
    brain_normalize_type_t type;       /**< Active normalization type */
    size_t num_features;               /**< Number of features */
} brain_feature_normalizer_t;

//=============================================================================
// TEMPORAL BUFFERING
//=============================================================================

/**
 * @brief Create temporal buffer for neural activity
 *
 * WHAT: Initialize multi-scale temporal buffering
 * WHY:  Enable temporal context in cognitive processing
 * HOW:  Create sliding windows and integration buffers
 *
 * @param num_channels Number of neural channels to buffer
 * @param size_preset Buffer size preset
 * @return Temporal buffer or NULL on failure
 */
brain_temporal_buffer_t* brain_create_temporal_buffer(
    size_t num_channels,
    brain_buffer_size_t size_preset
);

/**
 * @brief Destroy temporal buffer
 *
 * @param buffer Buffer to destroy (can be NULL)
 */
void brain_destroy_temporal_buffer(brain_temporal_buffer_t* buffer);

/**
 * @brief Buffer neural activity at current timestep
 *
 * WHAT: Add current neural activity to temporal buffers
 * WHY:  Maintain history for temporal processing
 * HOW:  Update all buffer components
 *
 * @param buffer Temporal buffer
 * @param activity Array of neural activities
 * @param num_channels Number of channels
 * @param timestamp Current timestamp
 * @return true on success
 */
bool brain_buffer_activity(
    brain_temporal_buffer_t* buffer,
    const float* activity,
    size_t num_channels,
    uint64_t timestamp
);

/**
 * @brief Extract windowed features from buffered activity
 *
 * WHAT: Compute statistical features over temporal window
 * WHY:  Convert temporal buffer to feature vector
 * HOW:  Extract mean, variance, trends from windows
 *
 * @param buffer Temporal buffer
 * @param features Output feature array
 * @param max_features Maximum number of features to extract
 * @return Number of features extracted
 */
size_t brain_extract_windowed_features(
    const brain_temporal_buffer_t* buffer,
    float* features,
    size_t max_features
);

//=============================================================================
// FEATURE NORMALIZATION
//=============================================================================

/**
 * @brief Create feature normalizer
 *
 * WHAT: Initialize normalization for extracted features
 * WHY:  Stable learning requires normalized inputs
 * HOW:  Create appropriate normalizer based on type
 *
 * @param num_features Number of features to normalize
 * @param type Normalization type
 * @return Feature normalizer or NULL on failure
 */
brain_feature_normalizer_t* brain_create_feature_normalizer(
    size_t num_features,
    brain_normalize_type_t type
);

/**
 * @brief Destroy feature normalizer
 *
 * @param normalizer Normalizer to destroy (can be NULL)
 */
void brain_destroy_feature_normalizer(brain_feature_normalizer_t* normalizer);

/**
 * @brief Normalize extracted features
 *
 * WHAT: Apply normalization to feature vector
 * WHY:  Ensure features have appropriate scale for learning
 * HOW:  Use active normalizer to transform features in-place
 *
 * @param normalizer Feature normalizer
 * @param features Feature array (normalized in-place)
 * @param num_features Number of features
 * @return true on success
 */
bool brain_normalize_features(
    brain_feature_normalizer_t* normalizer,
    float* features,
    size_t num_features
);

//=============================================================================
// COMBINED OPERATIONS
//=============================================================================

/**
 * @brief Extract and normalize features in one operation
 *
 * WHAT: Combined feature extraction and normalization
 * WHY:  Convenient single-call operation for cognitive modules
 * HOW:  Extract features from buffer then normalize
 *
 * @param buffer Temporal buffer
 * @param normalizer Feature normalizer
 * @param features Output feature array (normalized)
 * @param max_features Maximum features to extract
 * @return Number of features extracted and normalized
 */
size_t brain_extract_and_normalize(
    const brain_temporal_buffer_t* buffer,
    brain_feature_normalizer_t* normalizer,
    float* features,
    size_t max_features
);

//=============================================================================
// PHASE 2: POPULATION CODING & FEATURE EXTRACTION
//=============================================================================

/**
 * @brief Spike-based feature extractor for neural populations
 *
 * WHAT: Extracts neural features from spike train populations
 * WHY:  Enable spike-based neural analysis in brain modules
 * HOW:  Wraps feature_extractor_t with brain-friendly interface
 */
typedef struct brain_spike_feature_extractor_struct* brain_spike_feature_extractor_t;

/**
 * @brief Population coding analyzer for distributed representations
 *
 * WHAT: Analyzes population codes and distributed representations
 * WHY:  Enable population-level neural analysis
 * HOW:  Wraps population_coding_encoder_t with brain interface
 */
typedef struct brain_population_analyzer_struct* brain_population_analyzer_t;

/**
 * @brief Create spike feature extractor for brain modules
 *
 * WHAT: Initialize feature extraction from spike trains
 * WHY:  Brain modules need spike-based feature analysis
 * HOW:  Create feature extractor with brain-appropriate config
 *
 * @param max_neurons Maximum number of neurons
 * @param compute_oscillations Enable oscillation power features
 * @param compute_synchrony Enable synchrony analysis
 * @return Feature extractor or NULL on failure
 */
brain_spike_feature_extractor_t brain_create_spike_feature_extractor(
    uint32_t max_neurons,
    bool compute_oscillations,
    bool compute_synchrony
);

/**
 * @brief Destroy spike feature extractor
 *
 * @param extractor Extractor to destroy (can be NULL)
 */
void brain_destroy_spike_feature_extractor(brain_spike_feature_extractor_t extractor);

/**
 * @brief Extract features from neural spike data
 *
 * WHAT: Compute population-level features from spike trains
 * WHY:  Convert spike data to analyzable features
 * HOW:  Compute firing rates, CV, synchrony, entropy, etc.
 *
 * @param extractor Feature extractor
 * @param spike_data Spike train data from neural population
 * @param features_out Output features structure
 * @return true on success
 */
bool brain_extract_spike_features(
    brain_spike_feature_extractor_t extractor,
    const spike_data_t* spike_data,
    middleware_features_t* features_out
);

/**
 * @brief Create population code analyzer
 *
 * WHAT: Initialize population coding analysis
 * WHY:  Analyze distributed representations in neural populations
 * HOW:  Create population encoder with sensible defaults
 *
 * @return Population analyzer or NULL on failure
 */
brain_population_analyzer_t brain_create_population_analyzer(void);

/**
 * @brief Destroy population analyzer
 *
 * @param analyzer Analyzer to destroy (can be NULL)
 */
void brain_destroy_population_analyzer(brain_population_analyzer_t analyzer);

/**
 * @brief Compute population vector from neural rates
 *
 * WHAT: Encode population activity as directional vector
 * WHY:  Extract direction/orientation from population codes
 * HOW:  Use population vector coding algorithm
 *
 * @param analyzer Population analyzer
 * @param rates Firing rates [num_neurons]
 * @param tuning_curves Neuron tuning curves [num_neurons]
 * @param num_neurons Number of neurons
 * @param vector_out Output population vector
 * @return true on success
 */
bool brain_compute_population_vector(
    brain_population_analyzer_t analyzer,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    vector3d_t* vector_out
);

/**
 * @brief Compute population synchrony index
 *
 * WHAT: Measure coordinated firing across neural population
 * WHY:  Detect functional connectivity and binding
 * HOW:  Compute pairwise spike train correlations
 *
 * @param analyzer Population analyzer
 * @param spike_trains Array of spike trains [num_neurons]
 * @param num_neurons Number of neurons
 * @param synchrony_out Output synchrony metrics
 * @return true on success
 */
bool brain_compute_population_synchrony(
    brain_population_analyzer_t analyzer,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    synchrony_result_t* synchrony_out
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_INTEGRATION_H
