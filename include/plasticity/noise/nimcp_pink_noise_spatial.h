//=============================================================================
// nimcp_pink_noise_spatial.h - Spatially Correlated Pink Noise
//=============================================================================
/**
 * @file nimcp_pink_noise_spatial.h
 * @brief Pink noise with spatial correlations across brain regions
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Generate pink noise with distance-dependent correlations
 * WHY:  Neural noise is spatially correlated in real brains:
 *       - Nearby regions have high correlation
 *       - Distant regions have low but non-zero correlation
 *       - Correlation falls off with distance (exponential decay)
 *       - Reflects anatomical connectivity patterns
 *
 * HOW:  Generate base noise, apply spatial filter based on distance matrix.
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Resting-state fMRI shows correlated fluctuations
 * - Functional connectivity follows anatomical pathways
 * - Default Mode Network has characteristic correlation patterns
 * - Correlation strength ∝ exp(-distance/λ) where λ is length constant
 *
 * MATHEMATICAL MODEL:
 * ===================
 * Given N brain regions with positions p_i and distances d_ij:
 *   Correlation(i,j) = exp(-d_ij / λ)
 * Generate noise: X = L × W where L = chol(Correlation), W = independent
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_PINK_NOISE_SPATIAL_H
#define NIMCP_PINK_NOISE_SPATIAL_H

#include <stdint.h>
#include <stdbool.h>
#include "plasticity/noise/nimcp_pink_noise.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define PINK_SPATIAL_MAX_REGIONS    128     /**< Maximum brain regions */

//=============================================================================
// Region Configuration
//=============================================================================

typedef struct {
    const char* name;           /**< Region name (e.g., "V1", "PFC") */
    float x, y, z;              /**< 3D position in brain space (mm) */
    float alpha;                /**< Spectral exponent for this region */
    float amplitude;            /**< Base noise amplitude */
} pink_spatial_region_t;

typedef enum {
    PINK_SPATIAL_DECAY_EXPONENTIAL,     /**< exp(-d/λ) */
    PINK_SPATIAL_DECAY_GAUSSIAN,        /**< exp(-d²/2σ²) */
    PINK_SPATIAL_DECAY_POWER_LAW,       /**< 1/(1 + d/λ)^β */
    PINK_SPATIAL_DECAY_CUSTOM           /**< User-defined matrix */
} pink_spatial_decay_t;

typedef struct {
    uint32_t num_regions;
    pink_spatial_region_t regions[PINK_SPATIAL_MAX_REGIONS];
    float length_constant;              /**< λ for correlation decay (mm) */
    float min_correlation;              /**< Minimum correlation floor */
    pink_spatial_decay_t decay_type;
    float power_law_exponent;           /**< β for power-law decay */
    float sample_rate;
    uint32_t seed;
} pink_spatial_config_t;

//=============================================================================
// State Structure
//=============================================================================

typedef struct {
    pink_spatial_config_t config;
    pink_noise_generator_t generators[PINK_SPATIAL_MAX_REGIONS];

    // Spatial correlation matrices
    float* distance_matrix;             /**< d_ij between regions */
    float* correlation_matrix;          /**< Correlation(i,j) */
    float* cholesky_matrix;             /**< L such that LL^T = Corr */
    uint32_t matrix_capacity;           /**< Allocated capacity for matrices */

    // UMM handles for matrices (optional)
    unified_mem_handle_t distance_matrix_handle;
    unified_mem_handle_t correlation_matrix_handle;
    unified_mem_handle_t cholesky_matrix_handle;
    unified_mem_manager_t mem_manager;  /**< UMM manager (NULL = use nimcp_malloc) */

    // Current noise values
    float current_values[PINK_SPATIAL_MAX_REGIONS];
    float independent_values[PINK_SPATIAL_MAX_REGIONS];

    bool matrices_valid;
    uint64_t sample_count;
} pink_spatial_t;

//=============================================================================
// API Functions
//=============================================================================

/**
 * @brief Get default configuration with common brain regions
 */
pink_spatial_config_t pink_spatial_default_config(void);

/**
 * @brief Create a predefined brain network configuration
 *
 * @param network_type One of: "visual", "motor", "default_mode", "salience"
 * @return Configuration for that network
 */
pink_spatial_config_t pink_spatial_network_config(const char* network_type);

/**
 * @brief Create spatial pink noise generator
 */
pink_spatial_t* pink_spatial_create(const pink_spatial_config_t* config);

/**
 * @brief Destroy spatial generator
 */
void pink_spatial_destroy(pink_spatial_t* spatial);

/**
 * @brief Add a brain region
 */
int pink_spatial_add_region(
    pink_spatial_t* spatial,
    const char* name,
    float x, float y, float z,
    float alpha,
    float amplitude
);

/**
 * @brief Compute correlation matrices from region positions
 */
int pink_spatial_compute_correlations(pink_spatial_t* spatial);

/**
 * @brief Set custom correlation matrix
 */
int pink_spatial_set_correlation_matrix(
    pink_spatial_t* spatial,
    const float* matrix
);

/**
 * @brief Generate next sample for all regions
 */
int pink_spatial_step(pink_spatial_t* spatial);

/**
 * @brief Get noise value for specific region
 */
float pink_spatial_get_region(
    const pink_spatial_t* spatial,
    uint32_t region_index
);

/**
 * @brief Get noise value by region name
 */
float pink_spatial_get_named(
    const pink_spatial_t* spatial,
    const char* name
);

/**
 * @brief Get all region values
 */
int pink_spatial_get_all(
    const pink_spatial_t* spatial,
    float* values
);

/**
 * @brief Get correlation between two regions
 */
float pink_spatial_get_correlation(
    const pink_spatial_t* spatial,
    uint32_t region_i,
    uint32_t region_j
);

/**
 * @brief Get distance between two regions
 */
float pink_spatial_get_distance(
    const pink_spatial_t* spatial,
    uint32_t region_i,
    uint32_t region_j
);

/**
 * @brief Reset all regions
 */
int pink_spatial_reset(pink_spatial_t* spatial, uint32_t new_seed);

/**
 * @brief Connect unified memory manager for CoW allocations
 *
 * WHAT: Attach UMM for memory-efficient matrix allocations
 * WHY:  Enable Copy-on-Write for distance/correlation/cholesky matrices
 * HOW:  Store manager reference, migrate existing allocations
 *
 * @param spatial Spatial noise generator
 * @param mem_manager Unified memory manager (NULL to disconnect)
 * @return 0 on success, negative on error
 */
int pink_spatial_connect_memory_manager(
    pink_spatial_t* spatial,
    unified_mem_manager_t mem_manager
);

/**
 * @brief Check if unified memory manager is connected
 *
 * @param spatial Spatial noise generator
 * @return true if UMM is connected
 */
bool pink_spatial_has_memory_manager(const pink_spatial_t* spatial);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_PINK_NOISE_SPATIAL_H
