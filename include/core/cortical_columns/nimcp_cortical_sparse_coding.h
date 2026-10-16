/**
 * @file nimcp_cortical_sparse_coding.h
 * @brief Sparse Distributed Representations for Cortical Columns
 * @version 1.0.0
 * @date 2025-12-15
 *
 * WHAT: Biologically-inspired sparse coding system for cortical column representations,
 *       implementing adaptive sparsity constraints, overcomplete dictionaries, and
 *       efficient coding principles found in mammalian sensory cortex.
 * WHY:  Biological neurons maintain sparse firing patterns (~1-5% active) which
 *       improves energy efficiency, noise robustness, and representational capacity.
 *       Sparse codes enhance feature selectivity and enable linear separability.
 * HOW:  Adaptive thresholds enforce population and lifetime sparsity targets via
 *       lateral inhibition and homeostatic plasticity. Reconstruction error and
 *       L1 penalties optimize sparse dictionaries following Olshausen & Field (1996).
 *
 * BIOLOGICAL BASIS:
 * ┌────────────────────────────────────────────────────────────────────┐
 * │                  CORTICAL SPARSE CODING PRINCIPLES                  │
 * ├────────────────────────────────────────────────────────────────────┤
 * │                                                                     │
 * │  SPARSE FIRING IN CORTEX:                                          │
 * │  • V1 simple cells: ~2-5% active during natural image viewing      │
 * │  • Sparse codes emerge from efficient coding principles            │
 * │  • Energy efficiency: ATP cost of action potentials ~high          │
 * │  • Metabolic constraint: Brain uses 20% of body's energy           │
 * │                                                                     │
 * │  MECHANISMS FOR SPARSITY:                                          │
 * │  • Lateral inhibition: Active columns suppress neighbors           │
 * │  • High firing thresholds: Only strong inputs trigger spikes       │
 * │  • Synaptic normalization: Prevents runaway excitation             │
 * │  • Homeostatic plasticity: Maintains target firing rates           │
 * │                                                                     │
 * │  COMPUTATIONAL ADVANTAGES:                                         │
 * │  • High dimensional: Overcomplete representations (M > N)          │
 * │  • Distributed: Information spread across many units               │
 * │  • Noise robust: Redundant encoding tolerates unit failures        │
 * │  • Energy efficient: Few active units reduce metabolic cost        │
 * │  • Linearly separable: Sparse high-dim codes aid classification    │
 * │                                                                     │
 * │  OLSHAUSEN & FIELD MODEL (1996):                                   │
 * │  • Natural images yield sparse, localized receptive fields         │
 * │  • Minimize: E = ||x - Wa||² + λΣ|aᵢ|  (reconstruction + L1)      │
 * │  • Emergent Gabor-like filters match V1 simple cells               │
 * │  • Dictionary W is overcomplete (more columns than inputs)         │
 * │                                                                     │
 * └────────────────────────────────────────────────────────────────────┘
 *
 * SPARSE CODING ARCHITECTURE:
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │               SPARSE DISTRIBUTED REPRESENTATION                  │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                  │
 * │  Input x (N-dim) → Encoding a (M-dim, M>N) → Reconstruction x̂  │
 * │                          ↓                                       │
 * │                    [Sparsity: 2-5%]                              │
 * │                          ↓                                       │
 * │              Adaptive Thresholds (homeostatic)                   │
 * │                          ↓                                       │
 * │              Lateral Inhibition (competitive)                    │
 * │                          ↓                                       │
 * │           Loss = ||x - Wa||² + λΣ|aᵢ|                           │
 * │                                                                  │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * MATHEMATICAL MODELS:
 *
 * 1. Population Sparsity (% active at any time):
 *    S_pop = (1/M) * Σᵢ H(|aᵢ| - θᵢ)
 *    where H = Heaviside step, θᵢ = adaptive threshold
 *
 * 2. Lifetime Sparsity (average activity per unit):
 *    S_life = (1/T) * Σₜ |aᵢ(t)|
 *
 * 3. Sparse Coding Loss:
 *    L = ||x - Wa||² + λ * Σᵢ|aᵢ|
 *    where x = input, W = dictionary, a = sparse code, λ = sparsity penalty
 *
 * 4. Adaptive Threshold Update (homeostatic):
 *    θᵢ(t+1) = θᵢ(t) + η * (S_target - S_actual)
 *
 * 5. Kurtosis (super-Gaussian distribution measure):
 *    K = E[(a - μ)⁴] / σ⁴
 *    Sparse codes have K > 3 (leptokurtic)
 *
 * 6. Lateral Inhibition:
 *    aᵢ' = max(0, aᵢ - γ * Σⱼ wᵢⱼ * aⱼ)
 *    where wᵢⱼ = inhibition strength (e.g., Mexican hat)
 *
 * TYPICAL PARAMETERS:
 * - Target sparsity: 0.02 - 0.05 (2-5% active)
 * - Overcomplete ratio: 2x - 4x (M = 2N to 4N)
 * - L1 penalty: λ = 0.01 - 0.1
 * - Adaptation rate: η = 0.001 - 0.01
 * - Inhibition strength: γ = 0.1 - 0.5
 *
 * PERFORMANCE:
 * - Encode: O(M) where M = number of columns
 * - Enforce sparsity: O(M log M) for k-WTA, O(M) for threshold
 * - Update thresholds: O(M)
 * - Compute stats: O(M)
 *
 * INTEGRATION:
 * - Compatible with nimcp_cortical_column.h (hypercolumns)
 * - Uses nimcp_memory.h for allocation
 * - Thread-safe with nimcp_platform_mutex
 * - Bio-async enabled via BIO_MODULE_CORTICAL_SPARSE
 *
 * REFERENCES:
 * - Olshausen & Field (1996) "Emergence of simple-cell receptive field properties"
 * - Olshausen & Field (1997) "Sparse coding with an overcomplete basis set"
 * - Willmore & Tolhurst (2001) "Characterizing the sparseness of neural codes"
 * - Lennie (2003) "The cost of cortical computation"
 * - Attwell & Laughlin (2001) "An energy budget for signaling in cortex"
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_CORTICAL_SPARSE_CODING_H
#define NIMCP_CORTICAL_SPARSE_CODING_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* Ternary support for sparse coefficients */
#include "utils/ternary/nimcp_ternary.h"

/* Cortical column modules */
#include "core/cortical_columns/nimcp_cortical_column.h"

/* Bio-async integration */
#include "async/nimcp_bio_async.h"

/* Utilities */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct cortical_sparse_coding_system cortical_sparse_coding_system_t;

/* Forward declare bio_module_context_t if not already defined */
#ifndef BIO_MODULE_CONTEXT_DECLARED
#define BIO_MODULE_CONTEXT_DECLARED
struct bio_module_context_struct;
typedef struct bio_module_context_struct* bio_module_context_t;
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SPARSE_CODING_MAX_COLUMNS           4096   /**< Max columns to track */
#define SPARSE_CODING_DEFAULT_SPARSITY      0.03f  /**< 3% typical cortical sparsity */
#define SPARSE_CODING_MIN_SPARSITY          0.01f  /**< 1% minimum sparsity */
#define SPARSE_CODING_MAX_SPARSITY          0.20f  /**< 20% maximum sparsity */
#define SPARSE_CODING_DEFAULT_LAMBDA        0.05f  /**< L1 penalty strength */
#define SPARSE_CODING_DEFAULT_OVERCOMPLETE  2.0f   /**< 2x overcomplete */
#define SPARSE_CODING_ADAPTATION_RATE       0.005f /**< Homeostatic adaptation */

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Sparsity constraint types
 *
 * BIOLOGICAL BASIS:
 * - Population: Instantaneous sparsity (% active neurons at time t)
 * - Lifetime: Temporal sparsity (average firing rate over time)
 * - Both: Constrain both population and lifetime sparsity
 */
typedef enum {
    SPARSITY_POPULATION = 0,    /**< Enforce population sparsity (% active) */
    SPARSITY_LIFETIME,          /**< Enforce lifetime sparsity (avg activity) */
    SPARSITY_BOTH               /**< Enforce both constraints */
} sparsity_type_t;

/**
 * @brief Sparse coding enforcement methods
 *
 * WHAT: Different algorithms for enforcing sparsity constraints
 * WHY:  Trade-offs between biological plausibility, speed, and quality
 */
typedef enum {
    SPARSITY_METHOD_THRESHOLD = 0,   /**< Hard threshold (fast, biological) */
    SPARSITY_METHOD_K_WTA,           /**< K-winners-take-all (precise) */
    SPARSITY_METHOD_SOFT_THRESHOLD,  /**< Soft threshold (differentiable) */
    SPARSITY_METHOD_LATERAL_INHIB    /**< Lateral inhibition (biological) */
} sparsity_method_t;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/**
 * @brief Sparse coding configuration
 *
 * WHAT: Parameters controlling sparse distributed representations
 * WHY:  Configure sparsity targets, penalties, and adaptation dynamics
 */
typedef struct {
    /* Sparsity constraints */
    sparsity_type_t sparsity_type;       /**< Type of sparsity to enforce */
    sparsity_method_t sparsity_method;   /**< Enforcement method */
    float target_sparsity;               /**< Target % active (0.02-0.05) */
    float sparsity_penalty;              /**< L1 penalty strength (lambda) */

    /* Dictionary parameters */
    float overcomplete_ratio;            /**< Columns/inputs ratio (>1 for overcomplete) */
    uint32_t num_input_dims;             /**< Input dimensionality (N) */
    uint32_t num_columns;                /**< Number of columns (M = N * ratio) */

    /* Lateral inhibition (if enabled) */
    bool enable_lateral_inhibition;      /**< Use lateral inhibition for sparsity */
    float inhibition_strength;           /**< Inhibition weight (gamma) */
    float inhibition_radius;             /**< Spatial inhibition radius (mm) */

    /* Homeostatic adaptation */
    bool enable_homeostasis;             /**< Adapt thresholds to maintain sparsity */
    float adaptation_rate;               /**< Threshold adaptation rate (eta) */
    float adaptation_time_constant;      /**< Time constant for adaptation (ms) */

    /* K-WTA parameters */
    uint32_t k_winners;                  /**< Number of winners (if K_WTA method) */

    /* Reconstruction */
    bool track_reconstruction_error;     /**< Compute reconstruction loss */
    float reconstruction_weight;         /**< Weight for reconstruction term */

    /* Statistics */
    bool compute_kurtosis;               /**< Track kurtosis (expensive) */
    uint32_t stats_window_size;          /**< Window for lifetime stats */

    /* Bio-async */
    bool enable_bio_async;               /**< Enable bio-async messaging */

    // NIMCP 2.10: Ternary sparse coding configuration
    // WHAT: Enable ternary sparse coefficients {-1, 0, +1}
    // WHY:  Memory efficiency (20x savings) and natural sparse representation
    // HOW:  Threshold-based quantization of continuous activations
    //
    // BIOLOGICAL BASIS:
    // - Cortical sparse codes are inherently discrete (spike/no-spike)
    // - Many sparse coding models use binary/ternary representations
    // - Sign information (excitatory/inhibitory) is important
    bool enable_ternary_coefficients;    /**< Enable ternary sparse coefficients */
    float ternary_positive_threshold;    /**< Threshold for +1 (excitatory) */
    float ternary_negative_threshold;    /**< Threshold for -1 (inhibitory) */
    ternary_pack_mode_t ternary_pack_mode; /**< Packing mode for ternary storage */
} sparse_coding_config_t;

/* ============================================================================
 * State Structures
 * ============================================================================ */

/**
 * @brief Per-column sparse coding state
 *
 * WHAT: Runtime state for each cortical column
 * WHY:  Track thresholds, activity history, and adaptation
 */
typedef struct {
    uint32_t column_id;                  /**< Column identifier */
    float activation_threshold;          /**< Adaptive firing threshold */
    float current_activation;            /**< Current activation level */
    float lifetime_activity;             /**< Running average activity */
    float inhibition_received;           /**< Total lateral inhibition */
    uint32_t activation_count;           /**< Total times activated */
    uint64_t last_active_time;           /**< Last activation timestamp (us) */
    bool is_currently_active;            /**< Active this timestep */

    // NIMCP 2.10: Ternary sparse coefficient
    trit_t ternary_activation;           /**< Ternary activation {-1, 0, +1} */
    bool use_ternary;                    /**< Using ternary representation */
} column_sparse_state_t;

/**
 * @brief Global sparse coding state
 *
 * WHAT: System-wide sparse coding statistics
 * WHY:  Monitor sparsity levels and reconstruction quality
 */
typedef struct {
    float current_population_sparsity;   /**< Current % active columns */
    float current_lifetime_sparsity;     /**< Average lifetime activity */
    float mean_activation_threshold;     /**< Mean threshold across columns */
    float reconstruction_error;          /**< ||x - Wa||² */
    float sparsity_loss;                 /**< λ * Σ|aᵢ| */
    float total_loss;                    /**< Reconstruction + sparsity */
    uint64_t update_count;               /**< Total updates performed */
    uint64_t last_update_time;           /**< Last update timestamp (us) */
} sparse_coding_state_t;

/* ============================================================================
 * Statistics Structures
 * ============================================================================ */

/**
 * @brief Sparse coding statistics
 *
 * WHAT: Detailed statistics about sparse representations
 * WHY:  Monitor quality, efficiency, and biological plausibility
 */
typedef struct {
    /* Sparsity metrics */
    float mean_sparsity;                 /**< Average sparsity level */
    float sparsity_variance;             /**< Variance in sparsity */
    float min_sparsity;                  /**< Minimum observed sparsity */
    float max_sparsity;                  /**< Maximum observed sparsity */

    /* Activity distribution */
    uint32_t active_columns;             /**< Currently active columns */
    uint32_t total_columns;              /**< Total number of columns */
    float activity_ratio;                /**< active / total */

    /* Quality metrics */
    float reconstruction_error;          /**< Reconstruction quality */
    float mean_reconstruction_error;     /**< Average over time */
    float kurtosis;                      /**< Distribution kurtosis (if enabled) */

    /* Threshold statistics */
    float mean_threshold;                /**< Average threshold */
    float threshold_variance;            /**< Threshold variance */
    float min_threshold;                 /**< Minimum threshold */
    float max_threshold;                 /**< Maximum threshold */

    /* Adaptation statistics */
    float threshold_adaptation_rate;     /**< Current adaptation rate */
    uint32_t homeostatic_adjustments;    /**< Total homeostatic updates */

    /* Efficiency metrics */
    float metabolic_cost;                /**< Estimated energy cost */
    float coding_efficiency;             /**< Bits per active column */
} sparse_coding_stats_t;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * @brief Create sparse coding system with default configuration
 *
 * WHAT: Initialize configuration with biologically-inspired defaults
 * WHY:  Provide sensible starting point for cortical sparse coding
 * HOW:  Set 3% population sparsity, 2x overcomplete, lateral inhibition
 *
 * @param config Configuration structure to initialize
 * @return 0 on success, negative error code on failure
 *
 * DEFAULT VALUES:
 * - target_sparsity: 0.03 (3%)
 * - overcomplete_ratio: 2.0
 * - sparsity_penalty: 0.05
 * - enable_lateral_inhibition: true
 * - enable_homeostasis: true
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: config is NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_default_config(sparse_coding_config_t* config);

/**
 * @brief Create sparse coding system
 *
 * WHAT: Allocate and initialize sparse coding system for cortical columns
 * WHY:  Enable sparse distributed representations with adaptive thresholds
 * HOW:  Allocate state arrays, initialize thresholds, setup bio-async
 *
 * @param config Configuration (NULL for defaults)
 * @return System handle or NULL on failure
 *
 * ALLOCATIONS:
 * - System structure
 * - Per-column state array (num_columns)
 * - Activity history buffer (stats_window_size)
 * - Mutex for thread safety
 *
 * ERROR CONDITIONS:
 * - NULL returned if allocation fails
 * - NULL if invalid config parameters
 *
 * COMPLEXITY: O(M) where M = num_columns
 * THREAD-SAFE: Yes (different systems)
 */
cortical_sparse_coding_system_t* cortical_sparse_create(
    const sparse_coding_config_t* config
);

/**
 * @brief Destroy sparse coding system
 *
 * WHAT: Free all resources and disconnect bio-async
 * WHY:  Clean shutdown without leaks
 * HOW:  Disconnect bio-async, destroy mutex, free arrays
 *
 * @param system System handle
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: No (must be sole owner)
 */
void cortical_sparse_destroy(cortical_sparse_coding_system_t* system);

/* ============================================================================
 * Sparsity Enforcement
 * ============================================================================ */

/**
 * @brief Enforce sparsity constraint on column activations
 *
 * WHAT: Apply sparsity constraint to reduce active columns to target percentage
 * WHY:  Implement biological sparse firing patterns for efficiency and selectivity
 * HOW:  Apply threshold, k-WTA, or lateral inhibition based on method
 *
 * @param system Sparse coding system
 * @param activations Input activation values (size = num_columns)
 * @param num_activations Number of activation values
 * @param output_activations Output sparse activations (size = num_columns)
 * @return 0 on success, negative error code on failure
 *
 * ALGORITHM (THRESHOLD method):
 * 1. For each column i:
 *    output[i] = (activations[i] > threshold[i]) ? activations[i] : 0
 * 2. Update current_population_sparsity
 *
 * ALGORITHM (K_WTA method):
 * 1. Sort activations in descending order
 * 2. Set top-K to their values, rest to 0
 * 3. Ensures exactly k_winners active
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system or arrays NULL
 * - NIMCP_ERROR_INVALID_PARAMETER: num_activations != num_columns
 *
 * COMPLEXITY:
 * - O(M) for threshold/lateral inhibition
 * - O(M log M) for K-WTA
 * THREAD-SAFE: Yes
 */
int cortical_sparse_enforce_sparsity(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    float* output_activations
);

/**
 * @brief Apply lateral inhibition to enforce sparsity
 *
 * WHAT: Suppress activations via Mexican hat lateral inhibition
 * WHY:  Biologically plausible competitive dynamics for sparsity
 * HOW:  Active columns inhibit neighbors proportional to distance
 *
 * @param system Sparse coding system
 * @param activations Input/output activations (modified in place)
 * @param num_activations Number of activations
 * @return 0 on success, negative error code on failure
 *
 * ALGORITHM:
 * For each column i:
 *   inhibition[i] = Σⱼ w(dᵢⱼ) * activations[j]
 *   where w(d) = γ * exp(-d²/2σ²) for d > 0 (Mexican hat)
 *   activations[i] = max(0, activations[i] - inhibition[i])
 *
 * COMPLEXITY: O(M²) full connectivity, can be sparsified
 * THREAD-SAFE: Yes
 */
int cortical_sparse_apply_lateral_inhibition(
    cortical_sparse_coding_system_t* system,
    float* activations,
    uint32_t num_activations
);

/* ============================================================================
 * Sparsity Metrics
 * ============================================================================ */

/**
 * @brief Compute population sparsity
 *
 * WHAT: Calculate instantaneous percentage of active columns
 * WHY:  Monitor whether system maintains target sparsity
 * HOW:  Count activations above threshold, divide by total
 *
 * @param system Sparse coding system
 * @param activations Current activation values
 * @param num_activations Number of activations
 * @return Population sparsity [0.0, 1.0] or -1.0 on error
 *
 * FORMULA:
 * S_pop = (# columns with |a| > θ) / M
 *
 * ERROR CONDITIONS:
 * - Returns -1.0 if system or activations NULL
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
float cortical_sparse_compute_population_sparsity(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations
);

/**
 * @brief Compute lifetime sparsity
 *
 * WHAT: Calculate temporal average activity per column
 * WHY:  Measure long-term firing rates (complementary to population)
 * HOW:  Average activity over stats window
 *
 * @param system Sparse coding system
 * @return Lifetime sparsity [0.0, 1.0] or -1.0 on error
 *
 * FORMULA:
 * S_life = (1/T) * Σₜ |aᵢ(t)|
 *
 * COMPLEXITY: O(1) (uses cached value)
 * THREAD-SAFE: Yes
 */
float cortical_sparse_compute_lifetime_sparsity(
    cortical_sparse_coding_system_t* system
);

/**
 * @brief Compute kurtosis of activation distribution
 *
 * WHAT: Measure "super-Gaussian-ness" of activation distribution
 * WHY:  Sparse codes should be leptokurtic (K > 3, heavy tails)
 * HOW:  Fourth moment: E[(a - μ)⁴] / σ⁴
 *
 * @param activations Activation values
 * @param num_activations Number of values
 * @return Kurtosis value (>3 for leptokurtic) or -1.0 on error
 *
 * INTERPRETATION:
 * - K = 3: Gaussian (not sparse)
 * - K > 3: Leptokurtic (sparse, good)
 * - K > 10: Very sparse (super-Gaussian)
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
float cortical_sparse_compute_kurtosis(
    const float* activations,
    uint32_t num_activations
);

/* ============================================================================
 * Threshold Adaptation (Homeostatic Plasticity)
 * ============================================================================ */

/**
 * @brief Update adaptive thresholds to maintain target sparsity
 *
 * WHAT: Adjust per-column thresholds via homeostatic plasticity
 * WHY:  Maintain stable sparsity levels despite input changes
 * HOW:  Increase threshold if too active, decrease if too quiet
 *
 * @param system Sparse coding system
 * @param current_sparsity Current observed sparsity
 * @return 0 on success, negative error code on failure
 *
 * ALGORITHM (per column):
 * θᵢ(t+1) = θᵢ(t) + η * (S_target - S_actual)
 *
 * If column over-active: increase threshold (suppress)
 * If column under-active: decrease threshold (excite)
 *
 * BIOLOGICAL BASIS: Synaptic scaling, intrinsic excitability
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system is NULL
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_update_thresholds(
    cortical_sparse_coding_system_t* system,
    float current_sparsity
);

/**
 * @brief Set threshold for specific column
 *
 * WHAT: Manually override adaptive threshold
 * WHY:  Initialize or adjust specific column properties
 * HOW:  Direct write to column state
 *
 * @param system Sparse coding system
 * @param column_id Column index
 * @param threshold New threshold value
 * @return 0 on success, negative error code on failure
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system is NULL
 * - NIMCP_ERROR_INVALID_PARAMETER: column_id out of range
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_set_column_threshold(
    cortical_sparse_coding_system_t* system,
    uint32_t column_id,
    float threshold
);

/* ============================================================================
 * Loss Functions
 * ============================================================================ */

/**
 * @brief Compute sparse coding loss (reconstruction + sparsity penalty)
 *
 * WHAT: Calculate total sparse coding objective: L = ||x - Wa||² + λΣ|aᵢ|
 * WHY:  Quantify quality of sparse representation (lower is better)
 * HOW:  Sum reconstruction error and L1-weighted sparsity
 *
 * @param system Sparse coding system
 * @param input Original input vector (x)
 * @param activations Sparse activations (a)
 * @param num_dims Dimensionality
 * @param dictionary Weight matrix W (optional, can be NULL)
 * @param reconstruction_error Output: reconstruction term (can be NULL)
 * @param sparsity_cost Output: sparsity term (can be NULL)
 * @return Total loss or -1.0 on error
 *
 * FORMULA:
 * L = ||x - Wa||² + λ * Σᵢ|aᵢ|
 *
 * If dictionary W is NULL, only sparsity cost is computed.
 *
 * ERROR CONDITIONS:
 * - Returns -1.0 if system, input, or activations NULL
 *
 * COMPLEXITY: O(N*M) for reconstruction, O(M) for sparsity
 * THREAD-SAFE: Yes
 */
float cortical_sparse_compute_loss(
    cortical_sparse_coding_system_t* system,
    const float* input,
    const float* activations,
    uint32_t num_dims,
    const float* dictionary,
    float* reconstruction_error,
    float* sparsity_cost
);

/* ============================================================================
 * Active Set Operations
 * ============================================================================ */

/**
 * @brief Get indices of currently active columns
 *
 * WHAT: Return list of columns with activation above threshold
 * WHY:  Identify which features are represented (sparse set)
 * HOW:  Scan activations, collect indices where active
 *
 * @param system Sparse coding system
 * @param activations Current activation values
 * @param num_activations Number of activations
 * @param active_indices Output array for active indices
 * @param max_active Maximum size of active_indices array
 * @param num_active Output: number of active columns found
 * @return 0 on success, negative error code on failure
 *
 * EXAMPLE:
 * If activations = [0.0, 0.8, 0.0, 0.5, 0.0] with threshold 0.1:
 * active_indices = [1, 3], num_active = 2
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: required arguments NULL
 * - NIMCP_ERROR_INVALID_PARAMETER: array size mismatch
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_get_active_set(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    uint32_t* active_indices,
    uint32_t max_active,
    uint32_t* num_active
);

/**
 * @brief Get activation values for active columns only
 *
 * WHAT: Extract sparse representation as value array
 * WHY:  Efficient storage of sparse codes
 * HOW:  Collect non-zero activations
 *
 * @param system Sparse coding system
 * @param activations Current activation values
 * @param num_activations Number of activations
 * @param active_values Output array for active values
 * @param max_active Maximum size of active_values array
 * @param num_active Output: number of active values
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_get_active_values(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    float* active_values,
    uint32_t max_active,
    uint32_t* num_active
);

/* ============================================================================
 * Statistics and Monitoring
 * ============================================================================ */

/**
 * @brief Get sparse coding statistics
 *
 * WHAT: Retrieve comprehensive statistics about sparse representations
 * WHY:  Monitor quality, efficiency, and biological plausibility
 * HOW:  Copy internal statistics to output structure
 *
 * @param system Sparse coding system
 * @param stats Output statistics structure
 * @return 0 on success, negative error code on failure
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system or stats NULL
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_get_stats(
    cortical_sparse_coding_system_t* system,
    sparse_coding_stats_t* stats
);

/**
 * @brief Get current system state
 *
 * WHAT: Retrieve current sparse coding state (sparsity, loss, etc.)
 * WHY:  Monitor real-time system dynamics
 * HOW:  Copy state structure
 *
 * @param system Sparse coding system
 * @param state Output state structure
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_get_state(
    cortical_sparse_coding_system_t* system,
    sparse_coding_state_t* state
);

/**
 * @brief Get per-column state
 *
 * WHAT: Retrieve state for specific column (threshold, activity, etc.)
 * WHY:  Inspect individual column dynamics
 * HOW:  Copy column state
 *
 * @param system Sparse coding system
 * @param column_id Column index
 * @param state Output column state
 * @return 0 on success, negative error code on failure
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system or state NULL
 * - NIMCP_ERROR_INVALID_PARAMETER: column_id out of range
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_get_column_state(
    cortical_sparse_coding_system_t* system,
    uint32_t column_id,
    column_sparse_state_t* state
);

/* ============================================================================
 * Bio-async Integration
 * ============================================================================ */

/**
 * @brief Connect to bio-async router
 *
 * WHAT: Register sparse coding system with bio-async messaging
 * WHY:  Enable inter-module communication via biological channels
 * HOW:  Register as BIO_MODULE_CORTICAL_SPARSE, allocate inbox
 *
 * @param system Sparse coding system
 * @return 0 on success, negative error code on failure
 *
 * MODULE ID: BIO_MODULE_CORTICAL_SPARSE (0x014C)
 * INBOX SIZE: 32 messages
 *
 * ERROR CONDITIONS:
 * - NIMCP_ERROR_NULL_POINTER: system is NULL
 * - NIMCP_ERROR_INVALID_STATE: already connected
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_connect_bio_async(
    cortical_sparse_coding_system_t* system
);

/**
 * @brief Disconnect from bio-async router
 *
 * WHAT: Unregister from bio-async messaging
 * WHY:  Clean shutdown
 * HOW:  Deregister module, free inbox
 *
 * @param system Sparse coding system
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_disconnect_bio_async(
    cortical_sparse_coding_system_t* system
);

/**
 * @brief Check bio-async connection status
 *
 * WHAT: Query whether system is connected to bio-async
 * WHY:  Verify integration before sending messages
 * HOW:  Check bio_async_enabled flag
 *
 * @param system Sparse coding system
 * @return true if connected, false otherwise
 *
 * COMPLEXITY: O(1)
 * THREAD-SAFE: Yes
 */
bool cortical_sparse_is_bio_async_connected(
    const cortical_sparse_coding_system_t* system
);

/* ============================================================================
 * Ternary Sparse Coding API (NIMCP 2.10)
 * ============================================================================ */

/**
 * @brief Quantize activations to ternary sparse code
 *
 * WHAT: Convert continuous activations to {-1, 0, +1}
 * WHY:  Memory efficiency and natural sparse representation
 * HOW:  Threshold-based quantization with positive/negative thresholds
 *
 * @param system Sparse coding system
 * @param activations Input continuous activations
 * @param num_activations Number of activations
 * @param ternary_output Output ternary activations (caller allocated)
 * @return 0 on success, negative error code on failure
 *
 * ALGORITHM:
 * For each activation a[i]:
 *   if a[i] >= positive_threshold: output[i] = +1
 *   else if a[i] <= -negative_threshold: output[i] = -1
 *   else: output[i] = 0
 *
 * COMPLEXITY: O(M) where M = num_columns
 * THREAD-SAFE: Yes
 */
int cortical_sparse_quantize_to_ternary(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    trit_t* ternary_output
);

/**
 * @brief Dequantize ternary sparse code to continuous
 *
 * WHAT: Convert ternary activations to continuous values
 * WHY:  Interface with continuous computations (reconstruction, etc.)
 * HOW:  Map {-1, 0, +1} to {-scale, 0, +scale}
 *
 * @param system Sparse coding system
 * @param ternary_input Input ternary activations
 * @param num_activations Number of activations
 * @param scale Scale factor for dequantization
 * @param continuous_output Output continuous activations (caller allocated)
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(M)
 * THREAD-SAFE: Yes
 */
int cortical_sparse_dequantize_from_ternary(
    cortical_sparse_coding_system_t* system,
    const trit_t* ternary_input,
    uint32_t num_activations,
    float scale,
    float* continuous_output
);

/**
 * @brief Enforce sparsity and return ternary output
 *
 * WHAT: Apply sparsity constraint and quantize to ternary
 * WHY:  Combined operation for efficiency
 * HOW:  Sparsity enforcement then threshold quantization
 *
 * @param system Sparse coding system
 * @param activations Input activation values
 * @param num_activations Number of activations
 * @param ternary_output Output ternary sparse activations
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(M) to O(M log M) depending on sparsity method
 * THREAD-SAFE: Yes
 */
int cortical_sparse_enforce_sparsity_ternary(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations,
    trit_t* ternary_output
);

/**
 * @brief Get ternary active set indices
 *
 * WHAT: Return indices of non-zero ternary activations
 * WHY:  Efficient sparse representation query
 * HOW:  Scan ternary array, collect non-zero indices
 *
 * @param ternary_activations Ternary activation array
 * @param num_activations Number of activations
 * @param active_indices Output array for active indices
 * @param max_active Maximum size of output array
 * @param num_active Output: number of active (non-zero) indices
 * @return 0 on success, negative error code on failure
 *
 * COMPLEXITY: O(M)
 */
int cortical_sparse_get_ternary_active_set(
    const trit_t* ternary_activations,
    uint32_t num_activations,
    uint32_t* active_indices,
    uint32_t max_active,
    uint32_t* num_active
);

/**
 * @brief Compute ternary sparse code sparsity
 *
 * WHAT: Calculate percentage of zero activations
 * WHY:  Monitor sparsity level of ternary codes
 * HOW:  Count zeros, divide by total
 *
 * @param ternary_activations Ternary activation array
 * @param num_activations Number of activations
 * @return Sparsity [0.0, 1.0] (fraction of zeros)
 *
 * COMPLEXITY: O(M)
 */
float cortical_sparse_compute_ternary_sparsity(
    const trit_t* ternary_activations,
    uint32_t num_activations
);

/**
 * @brief Get ternary activation distribution
 *
 * WHAT: Count distribution of {-1, 0, +1} values
 * WHY:  Analyze balance between excitatory/inhibitory activations
 * HOW:  Count occurrences of each value
 *
 * @param ternary_activations Ternary activation array
 * @param num_activations Number of activations
 * @param n_negative Output: count of -1 values
 * @param n_zero Output: count of 0 values
 * @param n_positive Output: count of +1 values
 *
 * COMPLEXITY: O(M)
 */
void cortical_sparse_get_ternary_distribution(
    const trit_t* ternary_activations,
    uint32_t num_activations,
    uint32_t* n_negative,
    uint32_t* n_zero,
    uint32_t* n_positive
);

/**
 * @brief Create packed ternary vector from activations
 *
 * WHAT: Create memory-efficient packed ternary vector
 * WHY:  20x memory savings for sparse code storage
 * HOW:  Use trit_vector_t with specified pack mode
 *
 * @param system Sparse coding system
 * @param activations Input continuous activations
 * @param num_activations Number of activations
 * @return Packed trit vector or NULL on failure
 *
 * COMPLEXITY: O(M)
 */
trit_vector_t* cortical_sparse_create_ternary_vector(
    cortical_sparse_coding_system_t* system,
    const float* activations,
    uint32_t num_activations
);

/**
 * @brief Ternary sparse dot product
 *
 * WHAT: Compute dot product using ternary sparse code
 * WHY:  Efficient computation with ternary weights
 * HOW:  Sum/subtract based on trit values
 *
 * @param ternary_code Ternary sparse code
 * @param num_elements Number of elements
 * @param values Value array to dot with
 * @return Dot product result
 *
 * ALGORITHM:
 * result = sum(values[i] if ternary[i] == +1) - sum(values[i] if ternary[i] == -1)
 *
 * COMPLEXITY: O(M)
 */
float cortical_sparse_ternary_dot(
    const trit_t* ternary_code,
    uint32_t num_elements,
    const float* values
);

/**
 * @brief Enable ternary mode for sparse coding system
 *
 * WHAT: Switch system to ternary sparse coding mode
 * WHY:  Enable memory-efficient sparse representations
 * HOW:  Set config flags and initialize ternary storage
 *
 * @param system Sparse coding system
 * @param positive_threshold Threshold for +1 quantization
 * @param negative_threshold Threshold for -1 quantization
 * @param pack_mode Packing mode for ternary storage
 * @return 0 on success, negative error code on failure
 */
int cortical_sparse_enable_ternary_mode(
    cortical_sparse_coding_system_t* system,
    float positive_threshold,
    float negative_threshold,
    ternary_pack_mode_t pack_mode
);

/**
 * @brief Disable ternary mode for sparse coding system
 *
 * WHAT: Switch system back to continuous sparse coding
 * WHY:  Allow fine-grained activation values
 * HOW:  Clear config flags
 *
 * @param system Sparse coding system
 * @return 0 on success, negative error code on failure
 */
int cortical_sparse_disable_ternary_mode(
    cortical_sparse_coding_system_t* system
);

/**
 * @brief Check if ternary mode is enabled
 *
 * @param system Sparse coding system
 * @return true if ternary mode enabled
 */
bool cortical_sparse_is_ternary_mode(
    const cortical_sparse_coding_system_t* system
);

#ifdef __cplusplus
}
#endif

#endif  // NIMCP_CORTICAL_SPARSE_CODING_H
