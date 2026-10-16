/**
 * @file nimcp_feature_hypercolumns.h
 * @brief Feature hypercolumn module for complete feature dimension coverage
 *
 * WHAT: Implements feature hypercolumns - organized ensembles of minicolumns
 * providing complete coverage of feature dimensions (orientation, spatial
 * frequency, color, direction, disparity, etc.) at a single spatial location.
 *
 * WHY: Enables systematic representation of sensory features through
 * topographically organized columns with overlapping tuning curves, supporting
 * population coding, sparse representation, and feature decoding.
 *
 * HOW: Uses Gaussian/von Mises tuning curves for feature selectivity,
 * competitive dynamics for sparsity, and population vector decoding for
 * feature readout. Supports multi-dimensional feature spaces with independent
 * or joint selectivity.
 *
 * @version 1.0.0
 * @date 2025-01-25
 */

#ifndef NIMCP_FEATURE_HYPERCOLUMNS_H
#define NIMCP_FEATURE_HYPERCOLUMNS_H

#include <stdint.h>
#include <stdbool.h>
#include "utils/platform/nimcp_platform_mutex.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Type Definitions
 * ========================================================================== */

/**
 * WHAT: Feature dimension types supported by hypercolumns
 * WHY: Different features require different mathematical treatments (circular,
 * linear, logarithmic) and biological implementations
 * HOW: Enum distinguishes feature spaces for appropriate tuning curve selection
 */
typedef enum {
    FEATURE_ORIENTATION,       /**< 0-180° (half-rotation symmetric) */
    FEATURE_DIRECTION,         /**< 0-360° (full rotation, motion) */
    FEATURE_SPATIAL_FREQ,      /**< Log scale (octaves, edge scale) */
    FEATURE_COLOR_HUE,         /**< Circular (0-360°, chromatic) */
    FEATURE_COLOR_SATURATION,  /**< Linear (0-1, chromatic intensity) */
    FEATURE_DISPARITY,         /**< Binocular depth (-max to +max) */
    FEATURE_TEMPORAL_FREQ,     /**< Hz (log scale, flicker/motion rate) */
    FEATURE_CUSTOM             /**< User-defined feature spaces */
} feature_type_t;

/**
 * WHAT: Feature dimension specification
 * WHY: Defines the parameter space covered by a set of tuned columns
 * HOW: Specifies value range, number of columns, and tuning properties
 */
typedef struct feature_dimension {
    feature_type_t type;       /**< Type of feature */
    float min_value;           /**< Minimum feature value */
    float max_value;           /**< Maximum feature value */
    uint32_t num_columns;      /**< Number of tuning preferences */
    bool is_circular;          /**< Wrap-around (orientation, hue, direction) */
    float tuning_width;        /**< Column bandwidth (sigma or kappa) */
} feature_dimension_t;

/**
 * WHAT: Single feature-selective minicolumn
 * WHY: Represents a neuron population tuned to specific feature values
 * HOW: Stores tuning preference, current activation, and synaptic weights
 */
typedef struct feature_column {
    float preferred_value;     /**< Center of tuning curve */
    float tuning_width;        /**< Bandwidth (sigma) */
    float activation;          /**< Current response level */
    float* weights;            /**< Input synaptic weights */
    uint32_t num_weights;      /**< Number of input connections */
} feature_column_t;

/**
 * WHAT: Statistics about hypercolumn activity
 * WHY: Monitoring population dynamics helps validate coding efficiency
 * HOW: Computes sparsity, selectivity, and entropy metrics
 */
typedef struct feature_hypercolumn_stats {
    float mean_activation;     /**< Average column response */
    float max_activation;      /**< Peak response */
    float sparsity;            /**< Fraction of active columns */
    float selectivity;         /**< Response sharpness */
    float entropy;             /**< Population entropy */
    uint32_t num_active;       /**< Count above threshold */
    uint32_t winner_index;     /**< Index of most active column */
} feature_hypercolumn_stats_t;

/**
 * WHAT: Complete feature hypercolumn structure
 * WHY: Provides full coverage of feature space(s) at spatial location
 * HOW: Contains array of tuned columns spanning all feature dimensions
 */
typedef struct feature_hypercolumn {
    feature_dimension_t* dimensions;   /**< Feature dimension specs */
    uint32_t num_dimensions;           /**< Number of feature axes */
    feature_column_t* columns;         /**< All feature columns */
    uint32_t total_columns;            /**< Product of dimension sizes */
    float position[3];                 /**< Spatial location (x, y, z) */
    float receptive_field_size;        /**< Spatial extent */
    nimcp_platform_mutex_t* mutex;     /**< Thread safety lock */
} feature_hypercolumn_t;

/* ============================================================================
 * Feature Dimension Configuration
 * ========================================================================== */

/**
 * WHAT: Creates a feature dimension specification
 * WHY: Configures the parameter space for a set of tuned columns
 * HOW: Initializes dimension struct with type, range, and column count
 *
 * @param type Feature type (orientation, color, etc.)
 * @param min_value Minimum feature value
 * @param max_value Maximum feature value
 * @param num_columns Number of tuning preferences spanning range
 * @return Initialized feature dimension
 */
feature_dimension_t feature_dimension_create(
    feature_type_t type,
    float min_value,
    float max_value,
    uint32_t num_columns
);

/**
 * WHAT: Sets circular (periodic) property of dimension
 * WHY: Circular features (orientation, hue) wrap around at boundaries
 * HOW: Enables modular distance computation for tuning curves
 *
 * @param dim Feature dimension to configure
 * @param is_circular True if feature wraps around
 */
void feature_dimension_set_circular(
    feature_dimension_t* dim,
    bool is_circular
);

/**
 * WHAT: Sets tuning curve bandwidth for all columns
 * WHY: Controls selectivity vs. coverage trade-off
 * HOW: Adjusts sigma (Gaussian) or kappa (von Mises) parameter
 *
 * @param dim Feature dimension to configure
 * @param width Tuning width (smaller = sharper tuning)
 */
void feature_dimension_set_tuning_width(
    feature_dimension_t* dim,
    float width
);

/* ============================================================================
 * Hypercolumn Creation and Destruction
 * ========================================================================== */

/**
 * WHAT: Creates feature hypercolumn with specified dimensions
 * WHY: General constructor for multi-dimensional feature spaces
 * HOW: Allocates columns spanning all dimension combinations
 *
 * @param dimensions Array of feature dimension specs
 * @param num_dimensions Number of feature axes
 * @return Allocated hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create(
    const feature_dimension_t* dimensions,
    uint32_t num_dimensions
);

/**
 * WHAT: Destroys feature hypercolumn and frees resources
 * WHY: Prevents memory leaks when hypercolumn no longer needed
 * HOW: Frees all columns, dimensions, and main structure
 *
 * @param hcol Hypercolumn to destroy
 */
void feature_hypercolumn_destroy(
    feature_hypercolumn_t* hcol
);

/* ============================================================================
 * Convenience Constructors
 * ========================================================================== */

/**
 * WHAT: Creates orientation hypercolumn (V1-style)
 * WHY: Common use case for visual processing
 * HOW: Creates circular dimension from 0-180° with specified granularity
 *
 * @param num_orientations Number of orientation columns
 * @return Orientation hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create_orientation(
    uint32_t num_orientations
);

/**
 * WHAT: Creates direction hypercolumn (MT/V5-style)
 * WHY: Motion processing requires full 360° coverage
 * HOW: Creates circular dimension from 0-360° for motion direction
 *
 * @param num_directions Number of direction columns
 * @return Direction hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create_direction(
    uint32_t num_directions
);

/**
 * WHAT: Creates spatial frequency hypercolumn
 * WHY: Multi-scale edge detection requires frequency selectivity
 * HOW: Creates log-spaced columns covering specified frequency range
 *
 * @param num_octaves Number of octave bands
 * @param min_freq Minimum spatial frequency (cycles/degree)
 * @param max_freq Maximum spatial frequency
 * @return Spatial frequency hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create_spatial_freq(
    uint32_t num_octaves,
    float min_freq,
    float max_freq
);

/**
 * WHAT: Creates 2D color hypercolumn (hue × saturation)
 * WHY: Color processing requires joint hue and saturation selectivity
 * HOW: Creates product space of circular hue and linear saturation
 *
 * @param num_hues Number of hue columns (circular)
 * @param num_saturations Number of saturation columns (linear)
 * @return Color hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create_color(
    uint32_t num_hues,
    uint32_t num_saturations
);

/**
 * WHAT: Creates binocular disparity hypercolumn
 * WHY: Depth perception requires disparity selectivity
 * HOW: Creates linear dimension from -max_disparity to +max_disparity
 *
 * @param num_disparities Number of disparity columns
 * @param max_disparity Maximum disparity (degrees)
 * @return Disparity hypercolumn or NULL on failure
 */
feature_hypercolumn_t* feature_hypercolumn_create_disparity(
    uint32_t num_disparities,
    float max_disparity
);

/* ============================================================================
 * Input Processing
 * ========================================================================== */

/**
 * WHAT: Processes pre-computed feature values to activate columns
 * WHY: When features already extracted, directly compute tuning responses
 * HOW: Evaluates tuning curves for each column given feature vector
 *
 * @param hcol Hypercolumn to process
 * @param input_features Feature values (one per dimension)
 * @param num_features Must match num_dimensions
 */
void feature_hypercolumn_process(
    feature_hypercolumn_t* hcol,
    const float* input_features,
    uint32_t num_features
);

/**
 * WHAT: Processes raw input through learned weights
 * WHY: When features not pre-extracted, use learned representations
 * HOW: Computes weighted sum then applies tuning curve
 *
 * @param hcol Hypercolumn to process
 * @param raw_input Raw sensory input vector
 * @param input_size Size of input vector
 */
void feature_hypercolumn_process_with_input(
    feature_hypercolumn_t* hcol,
    const float* raw_input,
    uint32_t input_size
);

/* ============================================================================
 * Competition and Sparsity
 * ========================================================================== */

/**
 * WHAT: Applies divisive normalization to activations
 * WHY: Normalizes population activity for contrast invariance
 * HOW: Divides each activation by sum of all activations
 *
 * @param hcol Hypercolumn to normalize
 */
void feature_hypercolumn_normalize(
    feature_hypercolumn_t* hcol
);

/**
 * WHAT: Applies softmax competition with temperature
 * WHY: Creates probabilistic distribution emphasizing winners
 * HOW: Exponentiates and normalizes with temperature parameter
 *
 * @param hcol Hypercolumn to process
 * @param temperature Controls competition strength (lower = sharper)
 */
void feature_hypercolumn_softmax(
    feature_hypercolumn_t* hcol,
    float temperature
);

/**
 * WHAT: Keeps only top-k most active columns
 * WHY: Enforces sparse coding for efficiency and noise robustness
 * HOW: Sorts activations and zeros all but top k
 *
 * @param hcol Hypercolumn to sparsify
 * @param k Number of winners to keep
 */
void feature_hypercolumn_k_winners(
    feature_hypercolumn_t* hcol,
    uint32_t k
);

/**
 * WHAT: Zeros activations below threshold
 * WHY: Removes noise and weak responses
 * HOW: Sets activation to 0 if below threshold value
 *
 * @param hcol Hypercolumn to threshold
 * @param threshold Minimum activation to keep
 */
void feature_hypercolumn_threshold(
    feature_hypercolumn_t* hcol,
    float threshold
);

/* ============================================================================
 * Decoding (Population Readout)
 * ========================================================================== */

/**
 * WHAT: Decodes feature values from population activity
 * WHY: Extracts represented features from distributed code
 * HOW: Uses population vector method (activity-weighted average)
 *
 * @param hcol Hypercolumn to decode
 * @param decoded_features Output array (size = num_dimensions)
 */
void feature_hypercolumn_decode(
    feature_hypercolumn_t* hcol,
    float* decoded_features
);

/**
 * WHAT: Decodes single dimension from population
 * WHY: When only one feature dimension needed
 * HOW: Computes weighted average of preferred values for that dimension
 *
 * @param hcol Hypercolumn to decode
 * @param dimension Index of dimension to decode
 * @return Decoded feature value
 */
float feature_hypercolumn_decode_single(
    feature_hypercolumn_t* hcol,
    uint32_t dimension
);

/**
 * WHAT: Decodes using explicit population vector method
 * WHY: Standard neural population decoding approach
 * HOW: decoded = Σ(activation_i × preferred_i) / Σ(activation_i)
 *
 * @param hcol Hypercolumn to decode
 * @param decoded_features Output array (size = num_dimensions)
 */
void feature_hypercolumn_decode_population_vector(
    feature_hypercolumn_t* hcol,
    float* decoded_features
);

/* ============================================================================
 * Activation Access
 * ========================================================================== */

/**
 * WHAT: Gets activation of specific column
 * WHY: Query individual column responses
 * HOW: Returns activation value at index
 *
 * @param hcol Hypercolumn to query
 * @param column_idx Column index
 * @return Activation value
 */
float feature_hypercolumn_get_activation(
    feature_hypercolumn_t* hcol,
    uint32_t column_idx
);

/**
 * WHAT: Copies all activations to output array
 * WHY: Export full population state
 * HOW: Memcpy of all column activations
 *
 * @param hcol Hypercolumn to query
 * @param activations Output array (size = total_columns)
 */
void feature_hypercolumn_get_all_activations(
    feature_hypercolumn_t* hcol,
    float* activations
);

/**
 * WHAT: Gets index of most active column
 * WHY: Winner-take-all readout
 * HOW: Finds argmax of activations
 *
 * @param hcol Hypercolumn to query
 * @return Index of winner column
 */
uint32_t feature_hypercolumn_get_winner(
    feature_hypercolumn_t* hcol
);

/**
 * WHAT: Gets indices and values of top-k columns
 * WHY: Sparse population readout
 * HOW: Partial sort to find k largest activations
 *
 * @param hcol Hypercolumn to query
 * @param k Number of top columns
 * @param indices Output indices (size = k)
 * @param activations Output activations (size = k)
 */
void feature_hypercolumn_get_top_k(
    feature_hypercolumn_t* hcol,
    uint32_t k,
    uint32_t* indices,
    float* activations
);

/* ============================================================================
 * Learning
 * ========================================================================== */

/**
 * WHAT: Updates weights using Hebbian learning
 * WHY: Strengthens connections based on co-activation
 * HOW: Δw = η × activation × input
 *
 * @param hcol Hypercolumn to train
 * @param input Input pattern
 * @param learning_rate Learning rate η
 */
void feature_hypercolumn_learn_hebbian(
    feature_hypercolumn_t* hcol,
    const float* input,
    float learning_rate
);

/**
 * WHAT: Updates weights using competitive learning
 * WHY: Develops distinct feature detectors through competition
 * HOW: Only winner and spatial neighbors update weights
 *
 * @param hcol Hypercolumn to train
 * @param input Input pattern
 * @param learning_rate Learning rate
 * @param neighborhood_sigma Width of neighborhood function
 */
void feature_hypercolumn_learn_competitive(
    feature_hypercolumn_t* hcol,
    const float* input,
    float learning_rate,
    float neighborhood_sigma
);

/* ============================================================================
 * Tuning Curve Access
 * ========================================================================== */

/**
 * WHAT: Samples tuning curves for a dimension
 * WHY: Visualize or analyze feature selectivity
 * HOW: Evaluates response across feature value range
 *
 * @param hcol Hypercolumn to query
 * @param dimension Dimension index
 * @param values Output feature values (size = num_points)
 * @param responses Output responses (size = num_points × num_columns)
 * @param num_points Number of sample points
 */
void feature_hypercolumn_get_tuning_curve(
    feature_hypercolumn_t* hcol,
    uint32_t dimension,
    float* values,
    float* responses,
    uint32_t num_points
);

/* ============================================================================
 * Multi-Column Operations
 * ========================================================================== */

/**
 * WHAT: Pools responses across multiple hypercolumns
 * WHY: Aggregate information from spatial neighborhood
 * HOW: Averages or max-pools responses across hypercolumn array
 *
 * @param hcols Array of hypercolumns
 * @param num_hcols Number of hypercolumns
 * @param pooled_output Output array (size = total_columns of hcols[0])
 */
void feature_hypercolumn_pool_responses(
    feature_hypercolumn_t** hcols,
    uint32_t num_hcols,
    float* pooled_output
);

/* ============================================================================
 * Statistics
 * ========================================================================== */

/**
 * WHAT: Computes comprehensive statistics about hypercolumn state
 * WHY: Monitor population dynamics and coding efficiency
 * HOW: Calculates sparsity, selectivity, entropy, and other metrics
 *
 * @param hcol Hypercolumn to analyze
 * @param stats Output statistics structure
 */
void feature_hypercolumn_get_stats(
    feature_hypercolumn_t* hcol,
    feature_hypercolumn_stats_t* stats
);

/**
 * WHAT: Computes activity sparsity
 * WHY: Measure coding efficiency (sparse = efficient)
 * HOW: Fraction of columns significantly active
 *
 * @param hcol Hypercolumn to analyze
 * @return Sparsity value (0 = dense, 1 = sparse)
 */
float feature_hypercolumn_compute_sparsity(
    feature_hypercolumn_t* hcol
);

/**
 * WHAT: Computes feature selectivity
 * WHY: Measure tuning sharpness (selective = specific)
 * HOW: Ratio of max to mean activation
 *
 * @param hcol Hypercolumn to analyze
 * @return Selectivity value (higher = sharper tuning)
 */
float feature_hypercolumn_compute_selectivity(
    feature_hypercolumn_t* hcol
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_FEATURE_HYPERCOLUMNS_H */
