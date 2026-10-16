/**
 * @file nimcp_population_coding.h
 * @brief Population coding: Distributed neural representations across neuron populations
 *
 * WHAT: Encode information in collective activity patterns of neural populations
 * WHY:  Population codes provide robustness, high dimensionality, and noise tolerance
 * HOW:  Vector sum, center of mass, PCA, synchrony analysis, distributed representations
 *
 * BIOLOGICAL BASIS:
 * - Motor cortex uses population vectors to encode movement direction
 * - Visual cortex represents orientation through population tuning curves
 * - Hippocampus uses place cell populations for spatial coding
 * - Population codes are noise-resistant through redundancy
 * - Synchrony indicates functional connectivity and coordinated processing
 *
 * ALGORITHMS:
 * 1. Vector Sum Coding: Combine tuned neuron responses into directional vector
 * 2. Center of Mass: Calculate population activity centroid for localization
 * 3. PCA: Extract principal components of population activity patterns
 * 4. Synchrony Analysis: Measure coordinated firing via cross-correlation
 * 5. Sparse Distributed Representations: High-dimensional binary codes
 *
 * @author NIMCP Development Team
 * @date 2025-01-19
 */

#ifndef NIMCP_POPULATION_CODING_H
#define NIMCP_POPULATION_CODING_H

#include "middleware/encoding/nimcp_rate_coding.h"  // For spike_train_t
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants and Configuration
//=============================================================================

/** Maximum population size supported */
#define POPULATION_MAX_NEURONS 10000

/** Maximum PCA components to extract */
#define POPULATION_MAX_PCA_COMPONENTS 10

/** Default number of PCA components */
#define POPULATION_DEFAULT_PCA_COMPONENTS 3

/** Minimum correlation window (ms) */
#define POPULATION_MIN_CORRELATION_WINDOW 1.0f

/** Maximum correlation window (ms) */
#define POPULATION_MAX_CORRELATION_WINDOW 1000.0f

/** Sparse coding sparsity threshold */
#define POPULATION_SPARSITY_THRESHOLD 0.1f

//=============================================================================
// Core Types
//=============================================================================

/**
 * WHAT: 3D vector representation
 * WHY:  Compact storage for directional population codes
 * HOW:  Cartesian coordinates plus magnitude
 */
typedef struct {
    float x, y, z;           /**< 3D Cartesian coordinates */
    float magnitude;         /**< Vector magnitude (norm) */
} vector3d_t;

/**
 * WHAT: PCA analysis result
 * WHY:  Dimensionality reduction of population activity
 * HOW:  Principal components with eigenvalues
 */
typedef struct {
    float* components;       /**< PCA components [n_components * dim] row-major */
    float* eigenvalues;      /**< Component variances (descending order) */
    float* mean;             /**< Mean activity vector [dim] */
    uint32_t n_components;   /**< Number of components extracted */
    uint32_t dim;            /**< Original dimensionality */
} pca_result_t;

/**
 * WHAT: Population synchrony metrics
 * WHY:  Quantify coordinated firing patterns
 * HOW:  Cross-correlation and temporal alignment
 */
typedef struct {
    float synchrony_index;   /**< [0, 1] coordinated firing measure */
    float mean_correlation;  /**< Average pairwise correlation */
    float peak_lag_ms;       /**< Time lag of peak correlation */
    float coherence;         /**< Phase coherence measure */
} synchrony_result_t;

/**
 * WHAT: Tuning curve for vector sum coding
 * WHY:  Each neuron has preferred direction for population vector
 * HOW:  Preferred direction (3D) and tuning width
 */
typedef struct {
    vector3d_t preferred_direction;  /**< Neuron's preferred direction */
    float tuning_width;              /**< Tuning curve width (radians) */
    float max_rate;                  /**< Maximum firing rate (Hz) */
} tuning_curve_t;

/**
 * WHAT: Population coding configuration
 * WHY:  Flexible encoding strategy parameters
 * HOW:  Control PCA components, correlation windows, sparsity, positional encoding
 */
typedef struct {
    uint32_t n_pca_components;      /**< Number of PCA components to extract */
    float correlation_window_ms;    /**< Time window for correlation (ms) */
    float synchrony_threshold;      /**< Threshold for synchrony detection */
    float sparsity_target;          /**< Target sparsity for distributed codes [0-1] */
    bool enable_pca;                /**< Enable PCA computation */
    bool enable_synchrony;          /**< Enable synchrony analysis */

    /* Positional Encoding Parameters */
    bool enable_positional_encoding; /**< Enable position-aware encoding */
    uint32_t pe_embedding_dim;      /**< Positional encoding dimension */
    float pe_frequency_base;        /**< Base for PE frequency scaling (default: 10000) */
    float position_weight;          /**< Weight for position in decoding [0-1] */
} population_coding_config_t;

/**
 * WHAT: Population coding encoder instance
 * WHY:  Maintain state across encoding operations
 * HOW:  Opaque handle pattern for encapsulation
 */
typedef struct population_coding_encoder_struct* population_coding_encoder_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * WHAT: Create population coding encoder
 * WHY:  Initialize encoder state and allocate resources
 * HOW:  Validate config, allocate working memory
 *
 * @param config Encoder configuration (NULL uses defaults)
 * @return Encoder handle or NULL on error
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes (each encoder has independent state)
 */
population_coding_encoder_t population_coding_create(
    const population_coding_config_t* config
);

/**
 * WHAT: Destroy population coding encoder
 * WHY:  Clean memory cleanup
 * HOW:  Free all allocated resources
 *
 * @param encoder Encoder to destroy (NULL is safe)
 *
 * COMPLEXITY: O(1)
 */
void population_coding_destroy(population_coding_encoder_t encoder);

/**
 * WHAT: Get default population coding configuration
 * WHY:  Provide sensible defaults
 * HOW:  Return pre-filled config struct
 *
 * @return Default configuration
 *
 * DEFAULTS:
 * - n_pca_components: 3
 * - correlation_window_ms: 50.0
 * - synchrony_threshold: 0.5
 * - sparsity_target: 0.1
 * - enable_pca: true
 * - enable_synchrony: true
 */
population_coding_config_t population_coding_default_config(void);

//=============================================================================
// Vector Sum Coding
//=============================================================================

/**
 * WHAT: Encode population activity as directional vector sum
 * WHY:  Extract direction from population of tuned neurons
 * HOW:  Weighted sum of preferred directions by firing rates
 *
 * ALGORITHM:
 * 1. For each neuron i: contribution = rate[i] * preferred_direction[i]
 * 2. Vector sum = sum(contributions)
 * 3. Normalize to unit vector
 * 4. Magnitude = norm(vector_sum) / max_possible
 *
 * BIOLOGICAL EXAMPLE: Motor cortex population vector for reach direction
 *
 * @param encoder Encoder instance
 * @param rates Firing rates of population [num_neurons]
 * @param tuning_curves Preferred directions [num_neurons]
 * @param num_neurons Number of neurons in population
 * @param vector_out Output population vector
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n) where n = num_neurons
 */
bool population_coding_encode_vector_sum(
    population_coding_encoder_t encoder,
    const float* rates,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    vector3d_t* vector_out
);

/**
 * WHAT: Decode vector to population firing rates
 * WHY:  Generate rates consistent with target direction
 * HOW:  Rate[i] = max_rate * cos(angle(vector, preferred[i]))
 *
 * @param encoder Encoder instance
 * @param vector Target direction vector
 * @param tuning_curves Neuron tuning curves [num_neurons]
 * @param num_neurons Number of neurons
 * @param rates_out Output firing rates [num_neurons]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n)
 */
bool population_coding_decode_vector_sum(
    population_coding_encoder_t encoder,
    const vector3d_t* vector,
    const tuning_curve_t* tuning_curves,
    uint32_t num_neurons,
    float* rates_out
);

//=============================================================================
// Center of Mass Coding
//=============================================================================

/**
 * WHAT: Calculate center of mass of population activity
 * WHY:  Localize activity peak in neural space
 * HOW:  Weighted average of neuron positions by firing rate
 *
 * ALGORITHM:
 * 1. COM = sum(rate[i] * position[i]) / sum(rate[i])
 *
 * BIOLOGICAL EXAMPLE: Place cell population coding of spatial location
 *
 * @param encoder Encoder instance
 * @param rates Firing rates [num_neurons]
 * @param positions Neuron positions in 3D space [num_neurons]
 * @param num_neurons Number of neurons
 * @param center_out Output center of mass
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n)
 */
bool population_coding_encode_center_of_mass(
    population_coding_encoder_t encoder,
    const float* rates,
    const vector3d_t* positions,
    uint32_t num_neurons,
    vector3d_t* center_out
);

//=============================================================================
// PCA Encoding
//=============================================================================

/**
 * WHAT: Extract principal components of population activity
 * WHY:  Reduce dimensionality while preserving variance
 * HOW:  Covariance matrix eigendecomposition
 *
 * ALGORITHM:
 * 1. Center data: X_centered = X - mean(X)
 * 2. Compute covariance: C = X_centered^T * X_centered / n
 * 3. Eigendecomposition: C = V * D * V^T
 * 4. Return top k eigenvectors (principal components)
 *
 * @param encoder Encoder instance
 * @param activity_matrix Population activity [num_samples * num_neurons]
 * @param num_samples Number of time samples
 * @param num_neurons Number of neurons
 * @param result_out Output PCA result (pre-allocated)
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n^2 * m + n^3) where n=neurons, m=samples
 * NOTE: Uses power iteration for top-k components (more efficient)
 */
bool population_coding_encode_pca(
    population_coding_encoder_t encoder,
    const float* activity_matrix,
    uint32_t num_samples,
    uint32_t num_neurons,
    pca_result_t* result_out
);

/**
 * WHAT: Project activity onto principal components
 * WHY:  Transform to lower-dimensional representation
 * HOW:  Matrix multiplication with PC basis
 *
 * @param encoder Encoder instance
 * @param activity Activity vector [num_neurons]
 * @param num_neurons Number of neurons
 * @param pca_result PCA result from encode_pca
 * @param projection_out Output projection [n_components]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(k * n) where k=components, n=neurons
 */
bool population_coding_project_pca(
    population_coding_encoder_t encoder,
    const float* activity,
    uint32_t num_neurons,
    const pca_result_t* pca_result,
    float* projection_out
);

//=============================================================================
// Synchrony Analysis
//=============================================================================

/**
 * WHAT: Compute population synchrony index
 * WHY:  Measure coordinated firing across population
 * HOW:  Average pairwise cross-correlation
 *
 * ALGORITHM:
 * 1. For each pair (i,j): compute cross-correlation within window
 * 2. synchrony = mean(max_correlation[i,j])
 * 3. peak_lag = mean(argmax(correlation[i,j]))
 *
 * @param encoder Encoder instance
 * @param spike_trains Array of spike trains [num_neurons]
 * @param num_neurons Number of neurons
 * @param result_out Output synchrony metrics
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n^2 * s) where n=neurons, s=spikes per neuron
 * NOTE: For large populations, samples random pairs
 */
bool population_coding_compute_synchrony(
    population_coding_encoder_t encoder,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    synchrony_result_t* result_out
);

/**
 * WHAT: Compute pairwise correlation matrix
 * WHY:  Detailed synchrony structure analysis
 * HOW:  Cross-correlation for all neuron pairs
 *
 * @param encoder Encoder instance
 * @param spike_trains Spike trains [num_neurons]
 * @param num_neurons Number of neurons
 * @param correlation_matrix_out Output [num_neurons * num_neurons]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n^2 * s)
 */
bool population_coding_correlation_matrix(
    population_coding_encoder_t encoder,
    spike_train_t* const * spike_trains,
    uint32_t num_neurons,
    float* correlation_matrix_out
);

//=============================================================================
// Distributed Representations
//=============================================================================

/**
 * WHAT: Encode sparse distributed representation
 * WHY:  High-dimensional binary codes with low overlap
 * HOW:  Threshold rates to create sparse binary pattern
 *
 * ALGORITHM:
 * 1. Sort neurons by firing rate
 * 2. Set top k% to 1, rest to 0 (k = sparsity_target * 100)
 * 3. Result: binary vector with k% active neurons
 *
 * @param encoder Encoder instance
 * @param rates Firing rates [num_neurons]
 * @param num_neurons Number of neurons
 * @param sparse_code_out Binary sparse code [num_neurons]
 * @return Number of active neurons
 *
 * COMPLEXITY: O(n log n) due to sorting
 */
uint32_t population_coding_encode_sparse(
    population_coding_encoder_t encoder,
    const float* rates,
    uint32_t num_neurons,
    bool* sparse_code_out
);

/**
 * WHAT: Calculate overlap between sparse codes
 * WHY:  Measure similarity of distributed representations
 * HOW:  Count shared active neurons
 *
 * @param code1 First sparse code [num_neurons]
 * @param code2 Second sparse code [num_neurons]
 * @param num_neurons Number of neurons
 * @return Overlap fraction [0, 1]
 *
 * COMPLEXITY: O(n)
 */
float population_coding_sparse_overlap(
    const bool* code1,
    const bool* code2,
    uint32_t num_neurons
);

//=============================================================================
// Utility Functions
//=============================================================================

/**
 * WHAT: Create PCA result structure
 * WHY:  Allocate storage for PCA analysis
 * HOW:  Allocate components, eigenvalues, mean vectors
 *
 * @param n_components Number of components
 * @param dim Original dimensionality
 * @return Allocated PCA result or NULL on error
 */
pca_result_t* population_coding_pca_result_create(
    uint32_t n_components,
    uint32_t dim
);

/**
 * WHAT: Destroy PCA result and free memory
 * WHY:  Clean cleanup
 *
 * @param result PCA result to destroy (NULL is safe)
 */
void population_coding_pca_result_destroy(pca_result_t* result);

/**
 * WHAT: Copy PCA result
 * WHY:  Duplicate PCA analysis for independent use
 *
 * @param src Source PCA result
 * @return Copy or NULL on error
 */
pca_result_t* population_coding_pca_result_copy(const pca_result_t* src);

/**
 * WHAT: Create vector3d from components
 * WHY:  Convenient constructor
 *
 * @param x X component
 * @param y Y component
 * @param z Z component
 * @return Initialized vector
 */
vector3d_t population_coding_vector3d_make(float x, float y, float z);

/**
 * WHAT: Calculate dot product of 3D vectors
 * WHY:  Vector operations for population coding
 *
 * @param v1 First vector
 * @param v2 Second vector
 * @return Dot product
 */
float population_coding_vector3d_dot(const vector3d_t* v1, const vector3d_t* v2);

/**
 * WHAT: Normalize vector to unit length
 * WHY:  Direction extraction
 *
 * @param v Vector to normalize (modified in-place)
 * @return true on success, false if zero vector
 */
bool population_coding_vector3d_normalize(vector3d_t* v);

//=============================================================================
// Positional Encoding Integration
//=============================================================================

/**
 * WHAT: Configure positional encoding for population coding
 * WHY:  Enable position-aware population representations
 * HOW:  Set PE parameters and initialize internal encoder
 *
 * BIOLOGICAL BASIS:
 * - Place cells have position-dependent tuning curves
 * - Population codes represent continuous variables across neurons
 * - Spatial organization affects neural tuning and connectivity
 *
 * @param encoder Encoder instance
 * @param embedding_dim Dimension of positional encodings
 * @param frequency_base Base for sinusoidal frequencies (default: 10000.0)
 * @param position_weight Weight for position in decoding [0-1]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(1) - configuration only, no computation
 */
bool population_coding_set_pe_config(
    population_coding_encoder_t encoder,
    uint32_t embedding_dim,
    float frequency_base,
    float position_weight
);

/**
 * WHAT: Apply positional encoding to neuron positions in population
 * WHY:  Encode spatial layout of neurons for position-aware coding
 * HOW:  Apply sinusoidal PE to each neuron position in population
 *
 * ALGORITHM:
 * 1. For each neuron at position i: PE(i, 2j) = sin(i / base^(2j/d))
 * 2. PE(i, 2j+1) = cos(i / base^(2j/d))
 * 3. Store position encodings for later use in decoding
 *
 * BIOLOGICAL BASIS:
 * - Encodes the topographic organization of neural populations
 * - Similar to how grid cells encode spatial position
 * - Preserves relative position information in continuous space
 *
 * @param encoder Encoder instance
 * @param num_neurons Number of neurons in population
 * @param position_encodings_out Output encodings [num_neurons * pe_dim]
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n * d) where n=num_neurons, d=embedding_dim
 */
bool population_coding_encode_neuron_positions(
    population_coding_encoder_t encoder,
    uint32_t num_neurons,
    float* position_encodings_out
);

/**
 * WHAT: Decode population activity with position weighting
 * WHY:  Incorporate spatial position information in decoding
 * HOW:  Weight decoding by position similarity using PE
 *
 * ALGORITHM:
 * 1. Compute standard population vector from rates
 * 2. For each neuron: position_similarity = dot(PE(i), query_position)
 * 3. weighted_rate = rate[i] * (1-w + w*position_similarity)
 * 4. Decode using weighted rates
 *
 * BIOLOGICAL BASIS:
 * - Models how spatial context modulates population readout
 * - Similar to attention mechanisms in cortical processing
 * - Neurons closer to target position contribute more
 *
 * @param encoder Encoder instance
 * @param rates Firing rates [num_neurons]
 * @param position_encodings Neuron position encodings [num_neurons * pe_dim]
 * @param num_neurons Number of neurons
 * @param query_position Query position encoding [pe_dim]
 * @param tuning_curves Tuning curves for vector decoding
 * @param vector_out Output decoded vector
 * @return true on success, false on error
 *
 * COMPLEXITY: O(n * d) where n=num_neurons, d=embedding_dim
 */
bool population_coding_position_aware_decode(
    population_coding_encoder_t encoder,
    const float* rates,
    const float* position_encodings,
    uint32_t num_neurons,
    const float* query_position,
    const tuning_curve_t* tuning_curves,
    vector3d_t* vector_out
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_POPULATION_CODING_H
