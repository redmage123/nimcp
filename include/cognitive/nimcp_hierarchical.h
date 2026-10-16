/**
 * @file nimcp_hierarchical.h
 * @brief Hierarchical Brain Regions - Brain-inspired multi-tasking architecture
 * @version 2.6.1
 * @date 2025-11-04
 *
 * Models biological brain organization with hierarchical regions
 * communicating through feedforward, feedback, and lateral connections.
 *
 * Architecture:
 *   Primary Sensory → Secondary → Association → Executive → Motor
 *
 * Biological inspiration:
 *   V1 → V2 → V4 → IT → PFC → Motor Cortex
 */

#ifndef NIMCP_HIERARCHICAL_H
#define NIMCP_HIERARCHICAL_H

#include "core/brain/nimcp_brain.h"
#include "utils/validation/nimcp_common.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

#ifndef HIERARCHICAL_MAX_NAME_LENGTH
#define HIERARCHICAL_MAX_NAME_LENGTH 128
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/**
 * @brief Brain region types (biological hierarchy)
 */
typedef enum {
    HIERARCHICAL_REGION_TYPE_SENSORY = 0,      /**< Primary sensory (V1, A1, S1) */
    HIERARCHICAL_REGION_TYPE_SECONDARY,        /**< Secondary processing (V2, A2) */
    HIERARCHICAL_REGION_TYPE_ASSOCIATION,      /**< Association cortex (IT, PPC) */
    HIERARCHICAL_REGION_TYPE_EXECUTIVE,        /**< Executive control (PFC, ACC) */
    HIERARCHICAL_REGION_TYPE_MOTOR,            /**< Motor/output (M1, premotor) */
    HIERARCHICAL_REGION_TYPE_SUBCORTICAL       /**< Subcortical (basal ganglia, hippocampus) */
} hierarchical_region_type_t;

/**
 * @brief Connection types between regions
 */
typedef enum {
    CONNECTION_FEEDFORWARD = 0,   /**< Bottom-up (sensory → higher) */
    CONNECTION_FEEDBACK,          /**< Top-down (higher → sensory) */
    CONNECTION_LATERAL            /**< Horizontal (same level) */
} connection_type_t;

/**
 * @brief Hierarchical brain region structure
 *
 * Represents a functional brain region (cortical area) in the hierarchical system
 */
typedef struct hierarchical_region {
    char name[HIERARCHICAL_MAX_NAME_LENGTH];      /**< Region identifier */
    hierarchical_region_type_t type;               /**< Functional type */
    uint32_t layer;                   /**< Hierarchical layer (0=lowest) */

    brain_t brain;                    /**< Underlying NIMCP brain */

    // Region properties
    float* activity;                  /**< Current activation pattern */
    uint32_t activity_size;           /**< Size of activity vector */

    float learning_rate_multiplier;   /**< Layer-specific learning rate */

    // Connections
    struct hierarchical_region** inputs;     /**< Input regions (feedforward) */
    uint32_t num_inputs;             /**< Number of input regions */

    struct hierarchical_region** feedback;   /**< Feedback regions (top-down) */
    uint32_t num_feedback;           /**< Number of feedback regions */

    struct hierarchical_region** lateral;    /**< Lateral regions (same level) */
    uint32_t num_lateral;            /**< Number of lateral regions */

    // Attention modulation
    bool attention_enabled;          /**< Can be modulated by attention */
    float attention_gain;            /**< Current attention weight */

    // Memory
    bool has_memory;                 /**< Persistent activity (working memory) */
    float* memory_buffer;            /**< Working memory buffer */
    float memory_decay;              /**< Decay rate (0-1) */

    // Statistics
    uint64_t activations;            /**< Number of forward passes */
    uint64_t updates;                /**< Number of learning updates */
} hierarchical_region_t;

/**
 * @brief Hierarchical brain system (opaque handle)
 *
 * Complete multi-region brain system with hierarchical organization.
 * Internal structure is hidden for encapsulation.
 */
typedef struct hierarchical_brain_internal* hierarchical_brain_t;

//=============================================================================
// Core Functions
//=============================================================================

/**
 * @brief Create a hierarchical brain system
 *
 * @param name System identifier
 * @param max_regions Maximum number of regions to allocate
 * @return Hierarchical brain system or NULL on error
 */
hierarchical_brain_t hierarchical_brain_create(const char* name, uint32_t max_regions);

/**
 * @brief Destroy hierarchical brain system
 *
 * @param hbrain Hierarchical brain to destroy
 */
void hierarchical_brain_destroy(hierarchical_brain_t hbrain);

//=============================================================================
// Region Management
//=============================================================================

/**
 * @brief Add a brain region to the hierarchy
 *
 * @param hbrain Hierarchical brain system
 * @param name Region name
 * @param type Region type (sensory, association, etc.)
 * @param layer Hierarchical layer (0 = lowest)
 * @param brain Underlying NIMCP brain
 * @return Region index or -1 on error
 */
int32_t hierarchical_add_region(
    hierarchical_brain_t hbrain,
    const char* name,
    hierarchical_region_type_t type,
    uint32_t layer,
    brain_t brain
);

/**
 * @brief Connect two regions
 *
 * @param hbrain Hierarchical brain system
 * @param source_idx Source region index
 * @param target_idx Target region index
 * @param conn_type Connection type (feedforward, feedback, lateral)
 * @return true on success, false on error
 */
bool hierarchical_connect_regions(
    hierarchical_brain_t hbrain,
    uint32_t source_idx,
    uint32_t target_idx,
    connection_type_t conn_type
);

/**
 * @brief Get region by name
 *
 * @param hbrain Hierarchical brain system
 * @param name Region name
 * @return Region pointer or NULL if not found
 */
hierarchical_region_t* hierarchical_get_region(hierarchical_brain_t hbrain, const char* name);

/**
 * @brief Get region by index
 *
 * @param hbrain Hierarchical brain system
 * @param index Region index
 * @return Region pointer or NULL if invalid
 */
hierarchical_region_t* hierarchical_get_region_by_index(hierarchical_brain_t hbrain, uint32_t index);

//=============================================================================
// Processing
//=============================================================================

/**
 * @brief Forward pass through entire hierarchy
 *
 * @param hbrain Hierarchical brain system
 * @param input Input to lowest layer
 * @param input_size Size of input
 * @return true on success, false on error
 */
bool hierarchical_forward(
    hierarchical_brain_t hbrain,
    const float* input,
    uint32_t input_size
);

/**
 * @brief Get output from specific region
 *
 * @param hbrain Hierarchical brain system
 * @param region_name Region to get output from
 * @param output Output buffer (caller allocated)
 * @param output_size Size of output buffer
 * @return true on success, false on error
 */
bool hierarchical_get_output(
    hierarchical_brain_t hbrain,
    const char* region_name,
    float* output,
    uint32_t output_size
);

/**
 * @brief Train hierarchy with labeled example
 *
 * @param hbrain Hierarchical brain system
 * @param input Input pattern
 * @param input_size Size of input
 * @param labels Labels for each task (array of strings)
 * @param num_labels Number of labels (should match output regions)
 * @param confidence Learning confidence (0-1)
 * @return true on success, false on error
 */
bool hierarchical_learn(
    hierarchical_brain_t hbrain,
    const float* input,
    uint32_t input_size,
    const char** labels,
    uint32_t num_labels,
    float confidence
);

//=============================================================================
// Neuromodulation
//=============================================================================

/**
 * @brief Set dopamine level (reward signal)
 *
 * @param hbrain Hierarchical brain system
 * @param level Dopamine level (0-1)
 */
void hierarchical_set_dopamine(hierarchical_brain_t hbrain, float level);

/**
 * @brief Set acetylcholine level (attention)
 *
 * @param hbrain Hierarchical brain system
 * @param level ACh level (0-1)
 */
void hierarchical_set_acetylcholine(hierarchical_brain_t hbrain, float level);

/**
 * @brief Modulate attention to specific region
 *
 * @param hbrain Hierarchical brain system
 * @param region_name Region to modulate
 * @param gain Attention gain (0-2, 1=normal)
 */
void hierarchical_modulate_attention(
    hierarchical_brain_t hbrain,
    const char* region_name,
    float gain
);

//=============================================================================
// Working Memory
//=============================================================================

/**
 * @brief Enable working memory for a region
 *
 * @param hbrain Hierarchical brain system
 * @param region_name Region name
 * @param buffer_size Size of memory buffer
 * @param decay_rate Decay per time step (0-1)
 * @return true on success, false on error
 */
bool hierarchical_enable_working_memory(
    hierarchical_brain_t hbrain,
    const char* region_name,
    uint32_t buffer_size,
    float decay_rate
);

/**
 * @brief Update working memory (decay old, maintain new)
 *
 * @param hbrain Hierarchical brain system
 */
void hierarchical_update_working_memory(hierarchical_brain_t hbrain);

//=============================================================================
// Accessors (for testing and introspection)
//=============================================================================

/**
 * @brief Get number of regions in hierarchy
 *
 * @param hbrain Hierarchical brain system
 * @return Number of regions, or 0 if invalid
 */
uint32_t hierarchical_get_num_regions(hierarchical_brain_t hbrain);

/**
 * @brief Get number of layers in hierarchy
 *
 * @param hbrain Hierarchical brain system
 * @return Number of layers, or 0 if invalid
 */
uint32_t hierarchical_get_num_layers(hierarchical_brain_t hbrain);

/**
 * @brief Get total forward passes
 *
 * @param hbrain Hierarchical brain system
 * @return Total forward passes count
 */
uint64_t hierarchical_get_total_forward_passes(hierarchical_brain_t hbrain);

/**
 * @brief Get region activation count
 *
 * @param region Region to query
 * @return Number of activations
 */
uint64_t hierarchical_region_get_activations(hierarchical_region_t* region);

/**
 * @brief Get region update count
 *
 * @param region Region to query
 * @return Number of updates
 */
uint64_t hierarchical_region_get_updates(hierarchical_region_t* region);

/**
 * @brief Get region input count
 *
 * @param region Region to query
 * @return Number of input connections
 */
uint32_t hierarchical_region_get_num_inputs(hierarchical_region_t* region);

/**
 * @brief Get maximum regions capacity
 *
 * @param hbrain Hierarchical brain system
 * @return Maximum regions capacity
 */
uint32_t hierarchical_get_max_regions(hierarchical_brain_t hbrain);

/**
 * @brief Get hierarchy name
 *
 * @param hbrain Hierarchical brain system
 * @return Name string or NULL if invalid
 */
const char* hierarchical_get_name(hierarchical_brain_t hbrain);

/**
 * @brief Get dopamine level
 *
 * @param hbrain Hierarchical brain system
 * @return Dopamine level (0-1)
 */
float hierarchical_get_dopamine(hierarchical_brain_t hbrain);

/**
 * @brief Get acetylcholine level
 *
 * @param hbrain Hierarchical brain system
 * @return Acetylcholine level (0-1)
 */
float hierarchical_get_acetylcholine(hierarchical_brain_t hbrain);

//=============================================================================
// Utilities
//=============================================================================

/**
 * @brief Get hierarchy statistics
 *
 * @param hbrain Hierarchical brain system
 * @param stats Output buffer for statistics (JSON format)
 * @param max_size Maximum size of stats buffer
 * @return Number of bytes written
 */
uint32_t hierarchical_get_stats(
    hierarchical_brain_t hbrain,
    char* stats,
    uint32_t max_size
);

/**
 * @brief Validate hierarchical brain structure
 *
 * @param hbrain Hierarchical brain system
 * @return true if valid, false otherwise
 */
bool hierarchical_validate(hierarchical_brain_t hbrain);

/**
 * @brief Save hierarchical brain to directory
 *
 * @param hbrain Hierarchical brain system
 * @param directory Directory to save all regions
 * @return true on success, false on error
 */
bool hierarchical_save(hierarchical_brain_t hbrain, const char* directory);

/**
 * @brief Load hierarchical brain from directory
 *
 * @param directory Directory containing saved regions
 * @return Hierarchical brain system or NULL on error
 */
hierarchical_brain_t hierarchical_load(const char* directory);

#ifdef __cplusplus
}
#endif

#endif /* NIMCP_HIERARCHICAL_H */
