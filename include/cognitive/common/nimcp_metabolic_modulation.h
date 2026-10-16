/**
 * @file nimcp_metabolic_modulation.h
 * @brief Shared metabolic modulation utilities for substrate bridges
 *
 * WHAT: Common utilities for ATP/fatigue-based modulation across substrate bridges
 * WHY: Eliminates code duplication (80+ files had identical clamp_f and modulation code)
 * HOW: Provides nimcp_clamp_f(), metabolic config structs, and generic compute functions
 *
 * This module extracts the common patterns found in substrate bridge implementations:
 * 1. clamp_f() utility function - now nimcp_clamp_f()
 * 2. Metabolic modulation configuration (ATP/fatigue sensitivity, min capacity)
 * 3. Generic metabolic effects computation from substrate state
 *
 * TENSOR SUPPORT (v2.7.0):
 * - metabolic_effects_tensor_t for batch processing across brain regions
 * - Tensor-based compute functions for SIMD-accelerated operations
 * - Backward-compatible scalar API preserved via metabolic_effects_t
 */

#ifndef NIMCP_METABOLIC_MODULATION_H
#define NIMCP_METABOLIC_MODULATION_H

#include <stdbool.h>
#include <stdint.h>
#include "utils/tensor/nimcp_tensor.h"
#include "utils/math/nimcp_math_helpers.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Metabolic Effect Indices (for tensor access)
 * ============================================================================ */

/**
 * @brief Indices for accessing metabolic effects in tensor form
 *
 * Use these indices with nimcp_tensor_get_flat() and nimcp_tensor_set_flat()
 * on metabolic_effects_tensor_t::effects tensor.
 */
typedef enum {
    METABOLIC_IDX_PRIMARY_ATP = 0,       /**< Primary ATP-modulated effect */
    METABOLIC_IDX_SECONDARY_ATP = 1,     /**< Secondary ATP-modulated effect */
    METABOLIC_IDX_PRIMARY_FATIGUE = 2,   /**< Primary fatigue-modulated effect */
    METABOLIC_IDX_SECONDARY_FATIGUE = 3, /**< Secondary fatigue-modulated effect */
    METABOLIC_IDX_OVERALL_CAPACITY = 4,  /**< Average of all 4 effects */
    METABOLIC_EFFECT_COUNT = 5           /**< Total number of effect values */
} metabolic_effect_index_t;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Backward-compat alias for nimcp_clampf() (from nimcp_math_helpers.h)
 * @deprecated Use nimcp_clampf() directly.
 */
static inline float nimcp_clamp_f(float value, float min_val, float max_val) {
    return nimcp_clampf(value, min_val, max_val);
}

/**
 * @brief Clamp a double value between min and max bounds
 */
static inline double nimcp_clamp_d(double value, double min_val, double max_val) {
    return value < min_val ? min_val : (value > max_val ? max_val : value);
}

/**
 * @brief Clamp an integer value between min and max bounds
 */
static inline int nimcp_clamp_i(int value, int min_val, int max_val) {
    return value < min_val ? min_val : (value > max_val ? max_val : value);
}

/* ============================================================================
 * Metabolic Modulation Configuration
 * ============================================================================ */

/**
 * @brief Multipliers for metabolic effect computation
 *
 * Replaces the magic numbers (1.1f, 0.9f, 0.95f, 1.05f, etc.) found across
 * substrate bridges. Each bridge can customize these multipliers for its
 * specific cognitive function.
 */
typedef struct metabolic_effect_multipliers {
    float atp_primary_mult;      /**< Multiplier for primary ATP effect (default 1.0) */
    float atp_secondary_mult;    /**< Multiplier for secondary ATP effect (default 1.1) */
    float fatigue_primary_mult;  /**< Multiplier for primary fatigue effect (default 1.0) */
    float fatigue_secondary_mult;/**< Multiplier for secondary fatigue effect (default 0.9) */
} metabolic_effect_multipliers_t;

/**
 * @brief Common metabolic modulation configuration
 *
 * This struct captures the common configuration pattern found in all substrate
 * bridges. Bridge-specific configs can embed this as their first member for
 * easy conversion.
 */
typedef struct metabolic_modulation_config {
    bool enable_atp_modulation;      /**< Enable ATP-based modulation */
    bool enable_fatigue_modulation;  /**< Enable fatigue-based modulation */
    bool enable_bio_async;           /**< Enable bio-async messaging */
    float atp_sensitivity;           /**< ATP sensitivity multiplier (default 1.0) */
    float fatigue_sensitivity;       /**< Fatigue sensitivity multiplier (default 1.0) */
    float min_capacity;              /**< Minimum capacity floor (default 0.2) */
    metabolic_effect_multipliers_t multipliers;  /**< Effect computation multipliers */
} metabolic_modulation_config_t;

/**
 * @brief Generic metabolic effects (4 primary effects + overall capacity)
 *
 * Most substrate bridges compute exactly 4 effects:
 * - 2 ATP-modulated effects (primary_atp, secondary_atp)
 * - 2 fatigue-modulated effects (primary_fatigue, secondary_fatigue)
 * - 1 overall capacity (average of the 4)
 *
 * Bridge-specific effect structs can map their named fields to these generic ones.
 */
typedef struct metabolic_effects {
    float primary_atp;       /**< Primary ATP-modulated effect */
    float secondary_atp;     /**< Secondary ATP-modulated effect */
    float primary_fatigue;   /**< Primary fatigue-modulated effect */
    float secondary_fatigue; /**< Secondary fatigue-modulated effect */
    float overall_capacity;  /**< Average of all 4 effects */
} metabolic_effects_t;

/**
 * @brief Input metabolic state for effect computation
 *
 * Matches the fields extracted from substrate_metabolic_state_t
 */
typedef struct metabolic_input {
    float atp_level;          /**< Current ATP level [0, 1] */
    float metabolic_capacity; /**< Current metabolic capacity [0, 1] */
} metabolic_input_t;

/* ============================================================================
 * Tensor-Based Structures (Batch Processing)
 * ============================================================================ */

/**
 * @brief Tensor-based metabolic effects for batch processing
 *
 * WHAT: Represents metabolic effects as tensors for efficient batch computation
 * WHY: Enables SIMD-accelerated processing across multiple brain regions
 * HOW: Uses nimcp_tensor_t with shape [batch_size, METABOLIC_EFFECT_COUNT]
 *
 * For single-region processing: effects tensor has shape [5]
 * For batch processing: effects tensor has shape [num_regions, 5]
 *
 * Effect indices (see metabolic_effect_index_t):
 *   [0] primary_atp, [1] secondary_atp, [2] primary_fatigue,
 *   [3] secondary_fatigue, [4] overall_capacity
 */
typedef struct metabolic_effects_tensor {
    nimcp_tensor_t* effects;  /**< Shape: [batch, 5] or [5] - all effect values */
    uint32_t batch_size;      /**< Number of regions (1 for single-region) */
    bool owns_tensor;         /**< True if this struct owns the tensor memory */
} metabolic_effects_tensor_t;

/**
 * @brief Tensor-based metabolic input for batch processing
 *
 * WHAT: Input metabolic state as tensors for batch computation
 * WHY: Enables processing multiple regions simultaneously
 * HOW: Uses nimcp_tensor_t with shape [batch_size, 2] or [batch_size]
 *
 * For single input: tensors have shape [1] (scalar)
 * For batch input: tensors have shape [num_regions]
 */
typedef struct metabolic_input_tensor {
    nimcp_tensor_t* atp_levels;         /**< Shape: [batch] - ATP levels per region */
    nimcp_tensor_t* metabolic_capacities; /**< Shape: [batch] - capacities per region */
    uint32_t batch_size;                /**< Number of regions in batch */
} metabolic_input_tensor_t;

/**
 * @brief Tensor-based multipliers for batch processing
 *
 * WHAT: Effect multipliers as tensor for per-region customization
 * WHY: Allows different brain regions to have different modulation profiles
 * HOW: Shape [batch, 4] where each row is [atp_primary, atp_secondary,
 *      fatigue_primary, fatigue_secondary]
 */
typedef struct metabolic_multipliers_tensor {
    nimcp_tensor_t* multipliers;  /**< Shape: [batch, 4] or [4] */
    uint32_t batch_size;          /**< Number of regions */
} metabolic_multipliers_tensor_t;

/* ============================================================================
 * Default Configuration
 * ============================================================================ */

/**
 * @brief Get default metabolic modulation configuration
 *
 * Returns configuration matching the existing defaults across bridges:
 * - ATP and fatigue modulation enabled
 * - Bio-async disabled
 * - Sensitivities at 1.0
 * - Min capacity at 0.2
 * - Standard multipliers (1.0, 1.1, 1.0, 0.9)
 */
metabolic_modulation_config_t metabolic_modulation_default_config(void);

/**
 * @brief Get default effect multipliers
 *
 * Returns the standard multipliers:
 * - atp_primary_mult: 1.0
 * - atp_secondary_mult: 1.1
 * - fatigue_primary_mult: 1.0
 * - fatigue_secondary_mult: 0.9
 */
metabolic_effect_multipliers_t metabolic_default_multipliers(void);

/* ============================================================================
 * Effect Computation Functions
 * ============================================================================ */

/**
 * @brief Compute metabolic effects from input state and configuration
 *
 * This replaces the duplicate metabolic modulation code found in all substrate
 * bridge update functions. The pattern is:
 *
 * if (config->enable_atp_modulation) {
 *     effects->primary_atp = clamp_f(atp * config->atp_sensitivity, min, 1.0);
 *     effects->secondary_atp = clamp_f(atp * mult * config->atp_sensitivity, min, 1.0);
 * }
 * if (config->enable_fatigue_modulation) {
 *     effects->primary_fatigue = clamp_f(cap * config->fatigue_sensitivity, min, 1.0);
 *     effects->secondary_fatigue = clamp_f(cap * mult * config->fatigue_sensitivity, min, 1.0);
 * }
 * effects->overall_capacity = average(all 4);
 *
 * @param input Metabolic state input (ATP level and metabolic capacity)
 * @param config Modulation configuration
 * @param effects Output effects structure to populate
 * @return 0 on success, -1 on error
 */
int metabolic_compute_effects(
    const metabolic_input_t* input,
    const metabolic_modulation_config_t* config,
    metabolic_effects_t* effects
);

/**
 * @brief Compute single ATP-modulated effect value
 *
 * Convenience function for bridges that need to compute individual effect values.
 *
 * @param atp_level Current ATP level [0, 1]
 * @param sensitivity ATP sensitivity multiplier
 * @param effect_multiplier Additional multiplier for this specific effect
 * @param min_capacity Minimum capacity floor
 * @return Computed effect value clamped to [min_capacity, 1.0]
 */
float metabolic_compute_atp_effect(
    float atp_level,
    float sensitivity,
    float effect_multiplier,
    float min_capacity
);

/**
 * @brief Compute single fatigue-modulated effect value
 *
 * Convenience function for bridges that need to compute individual effect values.
 *
 * @param metabolic_capacity Current metabolic capacity [0, 1]
 * @param sensitivity Fatigue sensitivity multiplier
 * @param effect_multiplier Additional multiplier for this specific effect
 * @param min_capacity Minimum capacity floor
 * @return Computed effect value clamped to [min_capacity, 1.0]
 */
float metabolic_compute_fatigue_effect(
    float metabolic_capacity,
    float sensitivity,
    float effect_multiplier,
    float min_capacity
);

/**
 * @brief Initialize metabolic effects to full capacity (1.0)
 *
 * Sets all effect fields to 1.0, matching the default initialization
 * pattern in substrate bridge create functions.
 *
 * @param effects Effects structure to initialize
 */
void metabolic_effects_init_full(metabolic_effects_t* effects);

/**
 * @brief Compute overall capacity as average of 4 effects
 *
 * @param effects Effects structure with individual effects populated
 * @return Average of the 4 effect values
 */
float metabolic_compute_overall_capacity(const metabolic_effects_t* effects);

/* ============================================================================
 * Configuration Helpers
 * ============================================================================ */

/**
 * @brief Copy common config fields from bridge-specific config
 *
 * Utility to extract metabolic_modulation_config_t from bridge-specific
 * configuration structs that embed the same fields.
 *
 * @param enable_atp ATP modulation enabled flag
 * @param enable_fatigue Fatigue modulation enabled flag
 * @param enable_bio_async Bio-async enabled flag
 * @param atp_sensitivity ATP sensitivity value
 * @param fatigue_sensitivity Fatigue sensitivity value
 * @param min_capacity Minimum capacity value
 * @param multipliers Optional custom multipliers (NULL for defaults)
 * @return Populated configuration structure
 */
metabolic_modulation_config_t metabolic_config_from_fields(
    bool enable_atp,
    bool enable_fatigue,
    bool enable_bio_async,
    float atp_sensitivity,
    float fatigue_sensitivity,
    float min_capacity,
    const metabolic_effect_multipliers_t* multipliers
);

/* ============================================================================
 * Tensor-Based Effect Computation (Batch Processing)
 * ============================================================================ */

/**
 * @brief Create tensor-based effects structure
 *
 * WHAT: Allocate a metabolic_effects_tensor_t for batch processing
 * WHY: Enables efficient batch computation across multiple brain regions
 * HOW: Allocates tensor with shape [batch_size, 5]
 *
 * @param batch_size Number of brain regions (use 1 for single-region)
 * @return Allocated effects tensor, or NULL on failure
 */
metabolic_effects_tensor_t* metabolic_effects_tensor_create(uint32_t batch_size);

/**
 * @brief Destroy tensor-based effects structure
 *
 * @param effects Effects tensor to destroy (NULL-safe)
 */
void metabolic_effects_tensor_destroy(metabolic_effects_tensor_t* effects);

/**
 * @brief Create tensor-based input structure
 *
 * WHAT: Allocate input tensors for batch metabolic computation
 * WHY: Package input data for batch processing
 * HOW: Allocates atp_levels and metabolic_capacities tensors
 *
 * @param batch_size Number of brain regions
 * @return Allocated input tensor, or NULL on failure
 */
metabolic_input_tensor_t* metabolic_input_tensor_create(uint32_t batch_size);

/**
 * @brief Destroy tensor-based input structure
 *
 * @param input Input tensor to destroy (NULL-safe)
 */
void metabolic_input_tensor_destroy(metabolic_input_tensor_t* input);

/**
 * @brief Create tensor-based multipliers structure
 *
 * WHAT: Allocate multipliers tensor for per-region customization
 * WHY: Different brain regions may need different modulation profiles
 * HOW: Allocates tensor with shape [batch_size, 4]
 *
 * @param batch_size Number of brain regions
 * @return Allocated multipliers tensor, or NULL on failure
 */
metabolic_multipliers_tensor_t* metabolic_multipliers_tensor_create(uint32_t batch_size);

/**
 * @brief Destroy tensor-based multipliers structure
 *
 * @param mult Multipliers tensor to destroy (NULL-safe)
 */
void metabolic_multipliers_tensor_destroy(metabolic_multipliers_tensor_t* mult);

/**
 * @brief Initialize multipliers tensor with default values
 *
 * Sets all regions to use the standard multipliers:
 * [1.0, 1.1, 1.0, 0.9] for each region.
 *
 * @param mult Multipliers tensor to initialize
 * @return 0 on success, -1 on error
 */
int metabolic_multipliers_tensor_init_default(metabolic_multipliers_tensor_t* mult);

/**
 * @brief Set multipliers for a specific region
 *
 * @param mult Multipliers tensor
 * @param region_idx Region index (must be < batch_size)
 * @param atp_primary Primary ATP multiplier
 * @param atp_secondary Secondary ATP multiplier
 * @param fatigue_primary Primary fatigue multiplier
 * @param fatigue_secondary Secondary fatigue multiplier
 * @return 0 on success, -1 on error
 */
int metabolic_multipliers_tensor_set_region(
    metabolic_multipliers_tensor_t* mult,
    uint32_t region_idx,
    float atp_primary,
    float atp_secondary,
    float fatigue_primary,
    float fatigue_secondary
);

/**
 * @brief Initialize effects tensor to full capacity (1.0)
 *
 * Sets all effect values across all regions to 1.0.
 *
 * @param effects Effects tensor to initialize
 * @return 0 on success, -1 on error
 */
int metabolic_effects_tensor_init_full(metabolic_effects_tensor_t* effects);

/**
 * @brief Compute metabolic effects using tensor operations (batch)
 *
 * WHAT: Batch compute metabolic effects for multiple brain regions
 * WHY: SIMD-accelerated processing for efficient multi-region updates
 * HOW: Uses tensor multiply, clamp, and mean operations
 *
 * Formula for each region i:
 *   if (enable_atp_modulation):
 *     effects[i, 0] = clamp(atp[i] * sensitivity * mult[i, 0], min, 1.0)
 *     effects[i, 1] = clamp(atp[i] * sensitivity * mult[i, 1], min, 1.0)
 *   if (enable_fatigue_modulation):
 *     effects[i, 2] = clamp(cap[i] * sensitivity * mult[i, 2], min, 1.0)
 *     effects[i, 3] = clamp(cap[i] * sensitivity * mult[i, 3], min, 1.0)
 *   effects[i, 4] = mean(effects[i, 0:4])
 *
 * @param input Input tensor with ATP levels and metabolic capacities
 * @param config Modulation configuration (applies to all regions)
 * @param multipliers Per-region multipliers (NULL for default)
 * @param effects Output effects tensor to populate
 * @return 0 on success, -1 on error
 */
int metabolic_compute_effects_tensor(
    const metabolic_input_tensor_t* input,
    const metabolic_modulation_config_t* config,
    const metabolic_multipliers_tensor_t* multipliers,
    metabolic_effects_tensor_t* effects
);

/**
 * @brief Compute overall capacity for all regions in batch
 *
 * WHAT: Compute mean of first 4 effects for each region
 * WHY: Update overall_capacity column after individual effect changes
 * HOW: Uses tensor slicing and mean operation
 *
 * @param effects Effects tensor with individual effects populated
 * @return 0 on success, -1 on error
 */
int metabolic_compute_overall_capacity_tensor(metabolic_effects_tensor_t* effects);

/**
 * @brief Extract single-region effects from tensor to scalar struct
 *
 * WHAT: Convert tensor effects for one region to scalar metabolic_effects_t
 * WHY: Backward compatibility with existing bridge code
 * HOW: Reads tensor values at specified region index
 *
 * @param tensor_effects Source tensor effects
 * @param region_idx Region index to extract
 * @param scalar_effects Destination scalar effects struct
 * @return 0 on success, -1 on error
 */
int metabolic_effects_tensor_to_scalar(
    const metabolic_effects_tensor_t* tensor_effects,
    uint32_t region_idx,
    metabolic_effects_t* scalar_effects
);

/**
 * @brief Copy scalar effects into tensor at specified region
 *
 * WHAT: Set tensor effects for one region from scalar struct
 * WHY: Integrate scalar computations into batch processing
 * HOW: Writes scalar values to tensor at region index
 *
 * @param tensor_effects Destination tensor effects
 * @param region_idx Region index to set
 * @param scalar_effects Source scalar effects
 * @return 0 on success, -1 on error
 */
int metabolic_effects_scalar_to_tensor(
    metabolic_effects_tensor_t* tensor_effects,
    uint32_t region_idx,
    const metabolic_effects_t* scalar_effects
);

/**
 * @brief Get effect value from tensor
 *
 * @param effects Effects tensor
 * @param region_idx Region index
 * @param effect_idx Effect index (use metabolic_effect_index_t)
 * @return Effect value, or NaN on error
 */
float metabolic_effects_tensor_get(
    const metabolic_effects_tensor_t* effects,
    uint32_t region_idx,
    metabolic_effect_index_t effect_idx
);

/**
 * @brief Set effect value in tensor
 *
 * @param effects Effects tensor
 * @param region_idx Region index
 * @param effect_idx Effect index (use metabolic_effect_index_t)
 * @param value Value to set
 * @return 0 on success, -1 on error
 */
int metabolic_effects_tensor_set(
    metabolic_effects_tensor_t* effects,
    uint32_t region_idx,
    metabolic_effect_index_t effect_idx,
    float value
);

/**
 * @brief Create input tensor from scalar values
 *
 * WHAT: Convenience function to create single-region input
 * WHY: Easy migration path from scalar to tensor API
 * HOW: Creates batch_size=1 input tensor
 *
 * @param atp_level ATP level value
 * @param metabolic_capacity Metabolic capacity value
 * @return Allocated input tensor, or NULL on failure
 */
metabolic_input_tensor_t* metabolic_input_tensor_from_scalar(
    float atp_level,
    float metabolic_capacity
);

/**
 * @brief Set input values for a specific region
 *
 * @param input Input tensor
 * @param region_idx Region index
 * @param atp_level ATP level
 * @param metabolic_capacity Metabolic capacity
 * @return 0 on success, -1 on error
 */
int metabolic_input_tensor_set_region(
    metabolic_input_tensor_t* input,
    uint32_t region_idx,
    float atp_level,
    float metabolic_capacity
);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_METABOLIC_MODULATION_H */
