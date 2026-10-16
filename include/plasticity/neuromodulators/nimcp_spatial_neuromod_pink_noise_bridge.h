//=============================================================================
// nimcp_spatial_neuromod_pink_noise_bridge.h - Pink Noise Spatial Neuromod Bridge
//=============================================================================
/**
 * @file nimcp_spatial_neuromod_pink_noise_bridge.h
 * @brief Bridge integrating spatially-correlated pink noise with spatial neuromodulation
 * @version 1.0.0
 * @date 2025-12-21
 *
 * WHAT: Applies spatially-correlated 1/f noise to neuromodulator diffusion
 * WHY:  Realistic neural noise is both temporal (1/f) and spatial (correlated):
 *       - Pink noise provides multi-timescale variability
 *       - Spatial correlation reflects anatomical connectivity
 *       - Together: biologically realistic stochastic neuromodulator dynamics
 *
 * HOW:  Bridge connects pink_noise_spatial_t to spatial_neuromod_field_t:
 *       1. Generate spatially-correlated pink noise for brain regions
 *       2. Inject noise into diffusion dynamics as stochastic term
 *       3. Modulate diffusion coefficient and decay rate
 *
 * BIOLOGICAL BASIS:
 * =================
 * - Neuromodulator fluctuations exhibit 1/f spectrum (Grace & Bunney, 1984)
 * - Spatially correlated due to shared inputs (Montague et al., 2004)
 * - Noise amplitude ~5-15% of mean concentration
 * - Correlation length λ ≈ 10-50mm (functional connectivity scale)
 *
 * MATHEMATICAL MODEL:
 * ===================
 * Enhanced reaction-diffusion with stochastic term:
 *   dc/dt = D(x,t)*Laplacian(c) - k(x,t)*c + S + σ*ξ(x,t)
 *
 * Where:
 *   D(x,t) = D₀ × (1 + α_D × noise(x,t))  // Noise-modulated diffusion
 *   k(x,t) = k₀ × (1 + α_k × noise(x,t))  // Noise-modulated decay
 *   ξ(x,t) = pink noise with spatial correlation
 *   σ = noise amplitude
 *
 * INTEGRATION ARCHITECTURE:
 * =========================
 * pink_spatial_t (brain regions) → bridge → spatial_neuromod_field_t (neurons)
 *   - Maps brain regions to neuron populations
 *   - Propagates spatially-correlated noise to diffusion dynamics
 *   - Preserves spatial correlation structure
 *
 * USE CASES:
 * ==========
 * - Exploration in reinforcement learning (dopamine noise)
 * - Attention variability (acetylcholine fluctuations)
 * - Mood dynamics (serotonin noise)
 * - Arousal fluctuations (norepinephrine noise)
 *
 * @author NIMCP Development Team
 */

#ifndef NIMCP_SPATIAL_NEUROMOD_PINK_NOISE_BRIDGE_H
#define NIMCP_SPATIAL_NEUROMOD_PINK_NOISE_BRIDGE_H

#include <stdint.h>
#include "utils/bridge/nimcp_bridge_base.h"
#include <stdbool.h>
#include "plasticity/noise/nimcp_pink_noise_spatial.h"
#include "plasticity/neuromodulators/nimcp_spatial_neuromod.h"
#include "utils/memory/nimcp_unified_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#define SPATIAL_PINK_BRIDGE_MAX_REGION_NEURON_MAPS 128  /**< Max region→neuron mappings */

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Modulation mode for how pink noise affects diffusion
 */
typedef enum {
    PINK_MOD_ADDITIVE,           /**< Add noise to concentration: c += σ*ξ */
    PINK_MOD_MULTIPLICATIVE,     /**< Scale diffusion coeff: D *= (1 + α*ξ) */
    PINK_MOD_DECAY_RATE,         /**< Scale decay rate: k *= (1 + α*ξ) */
    PINK_MOD_SOURCE_TERM,        /**< Add to source: S += σ*ξ */
    PINK_MOD_HYBRID              /**< Combine additive + multiplicative */
} pink_modulation_mode_t;

/**
 * @brief Configuration for spatial pink noise neuromodulation bridge
 */
typedef struct {
    // === PINK NOISE CONFIGURATION ===
    pink_spatial_config_t pink_config;  /**< Configuration for spatial pink noise generator */

    // === MODULATION PARAMETERS ===
    pink_modulation_mode_t modulation_mode;  /**< How noise affects diffusion */
    float noise_amplitude;                   /**< σ: noise strength (0-1, default: 0.1) */
    float diffusion_modulation_strength;     /**< α_D: diffusion modulation (default: 0.2) */
    float decay_modulation_strength;         /**< α_k: decay modulation (default: 0.15) */

    // === SPATIAL MAPPING ===
    bool auto_map_regions_to_neurons;        /**< Auto-assign neurons to nearest regions */
    float region_influence_radius;           /**< Radius for region influence (mm, default: 20.0) */

    // === UPDATE CONTROL ===
    bool enabled;                            /**< Enable/disable bridge */
    uint32_t update_interval;                /**< Update noise every N timesteps (default: 1) */

} spatial_pink_bridge_config_t;

/**
 * @brief Mapping between brain region and neuron population
 *
 * WHAT: Associates spatial pink noise region with neuron subset
 * WHY:  Propagate regional noise to neurons in that region
 * HOW:  Each neuron assigned to nearest region (or custom mapping)
 */
typedef struct {
    uint32_t region_index;        /**< Index in pink_spatial_t */
    uint32_t* neuron_ids;         /**< Array of neuron IDs in this region */
    uint32_t num_neurons;         /**< Number of neurons in region */
    float region_weight;          /**< Influence weight [0, 1] */
} region_neuron_map_t;

//=============================================================================
// Bridge State
//=============================================================================

/**
 * @brief Spatial pink noise neuromodulation bridge state
 *
 * WHAT: Connects spatial pink noise to spatial neuromodulator field
 * WHY:  Inject spatially-correlated stochastic dynamics into diffusion
 * HOW:  Generate noise, map regions→neurons, apply to diffusion dynamics
 */
typedef struct {
    
    bridge_base_t base;                 /* MUST be first: base bridge infrastructure */
    // === CONNECTED SYSTEMS ===
    pink_spatial_t* pink_spatial;                    /**< Spatial pink noise generator */
    spatial_neuromod_field_t* neuromod_field;        /**< Spatial neuromodulator field */

    // === CONFIGURATION ===
    spatial_pink_bridge_config_t config;

    // === REGION-NEURON MAPPING ===
    region_neuron_map_t region_maps[SPATIAL_PINK_BRIDGE_MAX_REGION_NEURON_MAPS];
    uint32_t num_region_maps;                        /**< Number of active mappings */
    uint32_t* neuron_to_region;                      /**< [num_neurons] neuron→region index */
    float* neuron_region_weights;                    /**< [num_neurons] influence weight [0,1] */

    // === NOISE BUFFERS ===
    float* current_noise_values;                     /**< [num_neurons] current noise per neuron */
    float* diffusion_modulation;                     /**< [num_neurons] D modulation factor */
    float* decay_modulation;                         /**< [num_neurons] k modulation factor */

    // === STATISTICS ===
    uint64_t update_count;                           /**< Number of updates performed */
    float avg_noise_magnitude;                       /**< Average |noise| */
    float max_noise_magnitude;                       /**< Max |noise| */
    float avg_diffusion_modulation;                  /**< Average diffusion modulation */
    float noise_spatial_correlation;                 /**< Measured spatial correlation */

    // === STATE FLAGS ===
    bool is_connected;                               /**< Bridge is connected and active */
    bool auto_mapping_initialized;                   /**< Auto-mapping has been computed */

    // === UNIFIED MEMORY (OPTIONAL) ===
    unified_mem_manager_t mem_manager;               /**< UMM manager (NULL = use nimcp_malloc) */
    unified_mem_handle_t noise_buffer_handle;
    unified_mem_handle_t modulation_buffer_handle;

} spatial_pink_bridge_t;

//=============================================================================
// Lifecycle Functions
//=============================================================================

/**
 * @brief Get default configuration for spatial pink noise bridge
 *
 * WHAT: Returns sensible defaults for given neuromodulator type
 * WHY:  Different neuromodulators have different noise characteristics
 * HOW:  Calibrated from literature values
 *
 * DEFAULTS BY NEUROMODULATOR:
 * - Dopamine: amplitude=0.10, diffusion_mod=0.20, decay_mod=0.15
 * - Serotonin: amplitude=0.08, diffusion_mod=0.15, decay_mod=0.10
 * - Acetylcholine: amplitude=0.12, diffusion_mod=0.25, decay_mod=0.20
 * - Norepinephrine: amplitude=0.10, diffusion_mod=0.18, decay_mod=0.12
 *
 * @param neuromod_type Neuromodulator type
 * @return Default configuration
 */
spatial_pink_bridge_config_t spatial_pink_bridge_default_config(
    neuromodulator_type_t neuromod_type
);

/**
 * @brief Create spatial pink noise neuromodulation bridge
 *
 * WHAT: Allocates bridge state and initializes pink noise generator
 * WHY:  Prepare for spatial noise injection into neuromodulation
 * HOW:  Create pink_spatial_t, allocate buffers, initialize mappings
 *
 * COMPLEXITY: O(N) where N = num_neurons
 *
 * @param config Bridge configuration
 * @param num_neurons Number of neurons in neuromodulator field
 * @return Bridge handle, or NULL on error
 */
spatial_pink_bridge_t* spatial_pink_bridge_create(
    const spatial_pink_bridge_config_t* config,
    uint32_t num_neurons
);

/**
 * @brief Destroy spatial pink noise bridge
 *
 * WHAT: Frees all allocated memory
 * WHY:  Clean shutdown, prevent memory leaks
 * HOW:  Destroy pink_spatial_t, free buffers, clear mappings
 *
 * @param bridge Bridge to destroy (can be NULL)
 */
void spatial_pink_bridge_destroy(spatial_pink_bridge_t* bridge);

//=============================================================================
// Connection Functions
//=============================================================================

/**
 * @brief Connect bridge to spatial neuromodulator field
 *
 * WHAT: Associates bridge with target neuromodulator field
 * WHY:  Bridge needs reference to apply noise
 * HOW:  Store pointer, validate num_neurons matches
 *
 * @param bridge Spatial pink bridge
 * @param neuromod_field Spatial neuromodulator field
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_connect_neuromod(
    spatial_pink_bridge_t* bridge,
    spatial_neuromod_field_t* neuromod_field
);

/**
 * @brief Disconnect bridge from neuromodulator field
 *
 * WHAT: Removes association with neuromodulator field
 * WHY:  Allow dynamic enable/disable of noise injection
 * HOW:  Clear pointer, set is_connected = false
 *
 * @param bridge Spatial pink bridge
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_disconnect_neuromod(spatial_pink_bridge_t* bridge);

/**
 * @brief Check if bridge is connected
 *
 * @param bridge Spatial pink bridge
 * @return true if connected to neuromodulator field
 */
bool spatial_pink_bridge_is_connected(const spatial_pink_bridge_t* bridge);

//=============================================================================
// Region-Neuron Mapping
//=============================================================================

/**
 * @brief Add brain region for noise generation
 *
 * WHAT: Adds a brain region to pink noise generator
 * WHY:  Define spatial structure for noise generation
 * HOW:  Calls pink_spatial_add_region()
 *
 * @param bridge Spatial pink bridge
 * @param name Region name (e.g., "V1", "PFC")
 * @param x X position (mm)
 * @param y Y position (mm)
 * @param z Z position (mm)
 * @param alpha Spectral exponent for this region (default: 1.0)
 * @param amplitude Noise amplitude for this region (default: 0.1)
 * @return Region index, or negative on error
 */
int spatial_pink_bridge_add_region(
    spatial_pink_bridge_t* bridge,
    const char* name,
    float x, float y, float z,
    float alpha,
    float amplitude
);

/**
 * @brief Map neuron to brain region
 *
 * WHAT: Assigns neuron to a brain region for noise propagation
 * WHY:  Route regional noise to specific neurons
 * HOW:  Add neuron to region's neuron_ids array
 *
 * @param bridge Spatial pink bridge
 * @param neuron_id Neuron ID
 * @param region_index Region index (from add_region)
 * @param weight Influence weight [0, 1] (default: 1.0)
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_map_neuron_to_region(
    spatial_pink_bridge_t* bridge,
    uint32_t neuron_id,
    uint32_t region_index,
    float weight
);

/**
 * @brief Automatically map neurons to nearest brain regions
 *
 * WHAT: Assigns each neuron to nearest region based on distance
 * WHY:  Convenient auto-setup for spatial noise
 * HOW:  For each neuron, find nearest region, assign with distance-based weight
 *
 * ALGORITHM:
 * 1. For each neuron i:
 *    a. Compute distance to all regions: d_ij = ||neuron_pos - region_pos||
 *    b. Find nearest region: j* = argmin_j d_ij
 *    c. Assign neuron to region with weight: w = exp(-d_ij* / radius)
 *
 * COMPLEXITY: O(N × R) where N=neurons, R=regions
 *
 * REQUIREMENTS:
 * - config.auto_map_regions_to_neurons = true
 * - Neurons must have positions (requires neural_network_t)
 *
 * @param bridge Spatial pink bridge
 * @param network Neural network (for neuron positions)
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_auto_map_neurons(
    spatial_pink_bridge_t* bridge,
    neural_network_t network
);

//=============================================================================
// Update Functions
//=============================================================================

/**
 * @brief Update pink noise and apply to neuromodulator diffusion
 *
 * WHAT: Generates new spatial noise and modulates diffusion dynamics
 * WHY:  Inject stochastic variability into neuromodulator propagation
 * HOW:
 *   1. Step pink_spatial_t to generate new noise values
 *   2. Map region noise to neurons via neuron_to_region mapping
 *   3. Apply noise to diffusion based on modulation_mode
 *
 * ALGORITHM:
 * ```
 * // Step 1: Generate regional noise
 * pink_spatial_step(bridge->pink_spatial);
 *
 * // Step 2: Map to neurons
 * for (neuron i = 0; i < num_neurons; i++) {
 *     region_idx = neuron_to_region[i];
 *     noise[i] = pink_spatial_get_region(pink_spatial, region_idx);
 *     noise[i] *= neuron_region_weights[i];  // Apply influence weight
 * }
 *
 * // Step 3: Apply to diffusion
 * switch (modulation_mode) {
 *   case ADDITIVE:
 *     concentration[i] += noise_amplitude * noise[i];
 *   case MULTIPLICATIVE:
 *     diffusion_coeff[i] *= (1 + diffusion_mod_strength * noise[i]);
 *   case DECAY_RATE:
 *     decay_rate[i] *= (1 + decay_mod_strength * noise[i]);
 *   case HYBRID:
 *     // Apply both additive and multiplicative
 * }
 * ```
 *
 * COMPLEXITY: O(N) where N = num_neurons
 *
 * @param bridge Spatial pink bridge
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_update(spatial_pink_bridge_t* bridge);

/**
 * @brief Apply noise modulation to neuromodulator field
 *
 * WHAT: Applies pre-computed noise values to diffusion dynamics
 * WHY:  Decouple noise generation from application (flexibility)
 * HOW:  Modifies diffusion_coeff, decay_rate, or concentration based on mode
 *
 * NOTE: Call spatial_pink_bridge_update() to do both generation + application
 *
 * @param bridge Spatial pink bridge
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_apply_modulation(spatial_pink_bridge_t* bridge);

//=============================================================================
// Enable/Disable
//=============================================================================

/**
 * @brief Enable pink noise injection
 *
 * @param bridge Spatial pink bridge
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_enable(spatial_pink_bridge_t* bridge);

/**
 * @brief Disable pink noise injection (preserve state)
 *
 * @param bridge Spatial pink bridge
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_disable(spatial_pink_bridge_t* bridge);

/**
 * @brief Check if bridge is enabled
 *
 * @param bridge Spatial pink bridge
 * @return true if enabled
 */
bool spatial_pink_bridge_is_enabled(const spatial_pink_bridge_t* bridge);

//=============================================================================
// Query Functions
//=============================================================================

/**
 * @brief Get current noise value at neuron
 *
 * @param bridge Spatial pink bridge
 * @param neuron_id Neuron ID
 * @return Current noise value, or 0.0 on error
 */
float spatial_pink_bridge_get_noise(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get diffusion modulation factor at neuron
 *
 * @param bridge Spatial pink bridge
 * @param neuron_id Neuron ID
 * @return Diffusion modulation factor (1.0 = no modulation)
 */
float spatial_pink_bridge_get_diffusion_modulation(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get decay modulation factor at neuron
 *
 * @param bridge Spatial pink bridge
 * @param neuron_id Neuron ID
 * @return Decay modulation factor (1.0 = no modulation)
 */
float spatial_pink_bridge_get_decay_modulation(
    const spatial_pink_bridge_t* bridge,
    uint32_t neuron_id
);

/**
 * @brief Get bridge statistics
 *
 * @param bridge Spatial pink bridge
 * @param avg_noise Output: average noise magnitude
 * @param max_noise Output: max noise magnitude
 * @param spatial_correlation Output: spatial correlation coefficient
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_get_stats(
    const spatial_pink_bridge_t* bridge,
    float* avg_noise,
    float* max_noise,
    float* spatial_correlation
);

//=============================================================================
// Reset & Debugging
//=============================================================================

/**
 * @brief Reset pink noise generator (new seed)
 *
 * @param bridge Spatial pink bridge
 * @param new_seed New random seed
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_reset(spatial_pink_bridge_t* bridge, uint32_t new_seed);

/**
 * @brief Validate bridge state (for debugging)
 *
 * WHAT: Checks invariants (mappings valid, noise in bounds, etc.)
 * WHY:  Catch configuration/numerical errors early
 * HOW:  Scans mappings and buffers for invalid values
 *
 * @param bridge Spatial pink bridge
 * @return true if valid, false if errors detected
 */
bool spatial_pink_bridge_validate(const spatial_pink_bridge_t* bridge);

//=============================================================================
// Unified Memory Integration
//=============================================================================

/**
 * @brief Connect unified memory manager for CoW allocations
 *
 * WHAT: Attach UMM for memory-efficient buffer allocations
 * WHY:  Enable Copy-on-Write for noise/modulation buffers
 * HOW:  Store manager reference, migrate existing allocations
 *
 * @param bridge Spatial pink bridge
 * @param mem_manager Unified memory manager (NULL to disconnect)
 * @return 0 on success, negative on error
 */
int spatial_pink_bridge_connect_memory_manager(
    spatial_pink_bridge_t* bridge,
    unified_mem_manager_t mem_manager
);

/**
 * @brief Check if unified memory manager is connected
 *
 * @param bridge Spatial pink bridge
 * @return true if UMM is connected
 */
bool spatial_pink_bridge_has_memory_manager(const spatial_pink_bridge_t* bridge);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_SPATIAL_NEUROMOD_PINK_NOISE_BRIDGE_H
