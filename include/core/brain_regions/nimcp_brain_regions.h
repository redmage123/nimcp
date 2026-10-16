/**
 * @file nimcp_brain_regions.h
 * @brief Modular brain architecture with specialized regions
 *
 * DESIGN PHILOSOPHY:
 * - Modular: Brain organized into specialized functional regions
 * - Hierarchical: Regions contain layers, layers contain minicolumns
 * - Biologically inspired: Based on cortical organization
 * - Non-invasive: Works alongside existing neural_network_t
 * - Extensible: Easy to add new region types
 *
 * BRAIN ORGANIZATION:
 * ```
 * Brain
 *  ├── Visual Cortex (V1, V2, V4, MT)
 *  │   ├── Layer 2/3 (edge detectors, orientation-selective)
 *  │   ├── Layer 4 (input from thalamus)
 *  │   ├── Layer 5 (output to motor)
 *  │   └── Layer 6 (feedback to thalamus)
 *  ├── Auditory Cortex (A1, A2)
 *  │   ├── Frequency maps (tonotopic organization)
 *  │   └── Temporal processing
 *  ├── Motor Cortex (M1, premotor)
 *  │   ├── Pattern generators
 *  │   └── Motoneurons
 *  └── Association Areas
 *      ├── Prefrontal cortex
 *      └── Parietal cortex
 * ```
 *
 * USAGE EXAMPLE:
 * ```c
 * // Create a modular brain
 * brain_module_t* brain = brain_module_create(1000);
 *
 * // Add visual cortex region
 * brain_region_t* v1 = brain_region_create(REGION_VISUAL_V1, 200);
 * brain_module_add_region(brain, v1);
 *
 * // Add auditory cortex region
 * brain_region_t* a1 = brain_region_create(REGION_AUDITORY_A1, 150);
 * brain_module_add_region(brain, a1);
 *
 * // Connect regions (V1 → MT → motor)
 * brain_module_connect_regions(brain, v1->id, motor->id, 0.5f);
 *
 * // Process sensory input
 * brain_region_process_input(v1, visual_input, input_size, timestamp);
 * ```
 *
 * TDD STATUS: Header-first design, tests to be written
 */

#ifndef NIMCP_BRAIN_REGIONS_H
#define NIMCP_BRAIN_REGIONS_H

#include "utils/validation/nimcp_common.h"
#include "core/neuralnet/nimcp_neuralnet.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "core/neuron_types/nimcp_neuron_types.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// BRAIN REGION TYPES
// ============================================================================

/**
 * @brief Types of brain regions
 *
 * Organized by functional/anatomical areas
 */
typedef enum {
    // ========== VISUAL CORTEX ==========
    REGION_VISUAL_V1 = 0,        /**< Primary visual cortex (edge, orientation) */
    REGION_VISUAL_V2,            /**< Secondary visual (complex features) */
    REGION_VISUAL_V4,            /**< Color and shape */
    REGION_VISUAL_MT,            /**< Motion (middle temporal) */
    REGION_VISUAL_IT,            /**< Inferior temporal (object recognition) */

    // ========== AUDITORY CORTEX ==========
    REGION_AUDITORY_A1 = 10,     /**< Primary auditory cortex (frequency) */
    REGION_AUDITORY_A2,          /**< Secondary auditory */
    REGION_AUDITORY_BELT,        /**< Belt region (complex sounds) */
    REGION_AUDITORY_PARABELT,    /**< Parabelt (spatial, language) */

    // ========== MOTOR CORTEX ==========
    REGION_MOTOR_M1 = 20,        /**< Primary motor cortex */
    REGION_MOTOR_PREMOTOR,       /**< Premotor cortex */
    REGION_MOTOR_SMA,            /**< Supplementary motor area */

    // ========== SOMATOSENSORY ==========
    REGION_SOMATOSENSORY_S1 = 30, /**< Primary somatosensory */
    REGION_SOMATOSENSORY_S2,      /**< Secondary somatosensory */

    // ========== ASSOCIATION AREAS ==========
    REGION_PREFRONTAL = 40,      /**< Prefrontal cortex (executive) */
    REGION_PARIETAL,             /**< Parietal cortex (spatial) */
    REGION_TEMPORAL,             /**< Temporal cortex (memory) */

    // ========== SUBCORTICAL ==========
    REGION_THALAMUS = 50,        /**< Thalamus (sensory relay) */
    REGION_HIPPOCAMPUS,          /**< Hippocampus (memory) */
    REGION_BASAL_GANGLIA,        /**< Basal ganglia (motor control) */
    REGION_CEREBELLUM,           /**< Cerebellum (coordination) */

    REGION_TYPE_COUNT            /**< Total number of region types */
} brain_region_type_t;

/**
 * @brief Cortical layer types
 *
 * Based on Brodmann's cytoarchitecture
 */
typedef enum {
    LAYER_1 = 0,                 /**< Molecular layer (mostly dendrites) */
    LAYER_2,                     /**< External granular layer */
    LAYER_3,                     /**< External pyramidal layer */
    LAYER_4,                     /**< Internal granular layer (input) */
    LAYER_5,                     /**< Internal pyramidal layer (output) */
    LAYER_6,                     /**< Multiform layer (feedback) */
    LAYER_COUNT                  /**< Total number of layers */
} cortical_layer_t;

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * @brief Minicolumn structure (basic processing unit)
 *
 * A minicolumn is a vertical column of ~80-100 neurons spanning cortical layers
 */
typedef struct {
    uint32_t id;                        /**< Unique minicolumn ID */
    uint32_t region_id;                 /**< Parent region ID */

    // Neuron organization by layer
    uint32_t* layer_neuron_ids[LAYER_COUNT]; /**< Neuron IDs in each layer */
    uint32_t layer_neuron_counts[LAYER_COUNT]; /**< Count per layer */

    // Spatial position
    float x, y;                         /**< Position within region */

    // Functional properties
    float selectivity;                  /**< Feature selectivity (0-1) */
    float preferred_stimulus;           /**< Preferred stimulus value */
} brain_minicolumn_t;

/**
 * @brief Brain region structure
 *
 * Represents a functional/anatomical region of the brain
 */
typedef struct {
    uint32_t id;                        /**< Unique region ID */
    brain_region_type_t type;           /**< Type of region */
    char name[64];                      /**< Human-readable name */

    // Neuron organization
    neural_network_t network;           /**< Underlying neural network */
    uint32_t total_neurons;             /**< Total neurons in region */

    // Extended neuron types (for specialized processing)
    neuron_type_extended_t* neuron_extended_types;  /**< Extended type for each neuron */
    neuron_type_params_t* neuron_type_params;       /**< Type-specific parameters */

    // Spatial organization
    brain_minicolumn_t** minicolumns;   /**< Array of minicolumns */
    uint32_t num_minicolumns;           /**< Number of minicolumns */
    uint32_t minicolumns_x, minicolumns_y; /**< 2D grid dimensions */

    // Layer organization
    uint32_t layer_sizes[LAYER_COUNT];  /**< Neurons per layer */

    // Glial integration
    glial_integration_t* glial;         /**< Glial cells for this region */

    // Connectivity
    uint32_t* input_regions;            /**< IDs of input regions */
    uint32_t num_input_regions;
    uint32_t* output_regions;           /**< IDs of output regions */
    uint32_t num_output_regions;

    // Processing state
    uint64_t last_update;               /**< Last update timestamp */
    float activity_level;               /**< Current activity (0-1) */

    // Predictive processing extension (NULL if not enabled)
    void* predictive_extension;         /**< brain_region_predictive_t* (opaque) */

    // Thread safety
    nimcp_mutex_t lock;
} brain_region_t;
#define NIMCP_BRAIN_REGION_T_DEFINED

/**
 * @brief Inter-region connection
 */
typedef struct {
    uint32_t source_region_id;          /**< Source region */
    cortical_layer_t source_layer;      /**< Source layer */

    uint32_t target_region_id;          /**< Target region */
    cortical_layer_t target_layer;      /**< Target layer */

    float connection_strength;          /**< Overall strength (0-1) */
    uint32_t num_synapses;              /**< Number of synapses */

    // Projection type
    bool feedforward;                   /**< Feedforward projection */
    bool feedback;                      /**< Feedback projection */
    bool lateral;                       /**< Lateral within-region */
} brain_connection_t;

/**
 * @brief Modular brain system
 *
 * Top-level structure organizing multiple brain regions
 */
typedef struct {
    uint32_t id;                        /**< Brain ID */

    // Regions
    brain_region_t** regions;           /**< Array of brain regions */
    uint32_t num_regions;               /**< Number of regions */
    uint32_t max_regions;               /**< Capacity */

    // Inter-region connections
    brain_connection_t** connections;   /**< Inter-region connections */
    uint32_t num_connections;           /**< Number of connections */

    // Global state
    uint64_t current_time;              /**< Current simulation time (µs) */
    uint32_t total_neurons;             /**< Total neurons across all regions */

    // Configuration
    bool enable_plasticity;             /**< Enable learning */
    bool enable_glial;                  /**< Enable glial cells */

    // Thread safety
    nimcp_mutex_t lock;
} brain_module_t;
#define NIMCP_BRAIN_MODULE_T_DEFINED

// ============================================================================
// BRAIN MODULE MANAGEMENT
// ============================================================================

/**
 * @brief Create modular brain system
 *
 * @param max_regions Maximum number of regions to support
 * @return Brain module handle or NULL on error
 */
brain_module_t* brain_module_create(uint32_t max_regions);

/**
 * @brief Destroy brain module
 *
 * @param brain Brain module (NULL safe)
 */
void brain_module_destroy(brain_module_t* brain);

/**
 * @brief Add region to brain
 *
 * @param brain Brain module
 * @param region Region to add (ownership transferred)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t brain_module_add_region(brain_module_t* brain, brain_region_t* region);

/**
 * @brief Get region by ID
 *
 * @param brain Brain module
 * @param region_id Region ID
 * @return Region handle or NULL if not found
 */
brain_region_t* brain_module_get_region(brain_module_t* brain, uint32_t region_id);

/**
 * @brief Get region by type
 *
 * @param brain Brain module
 * @param type Region type
 * @return First region of this type, or NULL if not found
 */
brain_region_t* brain_module_get_region_by_type(brain_module_t* brain,
                                                 brain_region_type_t type);

// ============================================================================
// BRAIN REGION MANAGEMENT
// ============================================================================

/**
 * @brief Create brain region
 *
 * @param type Type of region
 * @param num_neurons Total neurons in region
 * @return Region handle or NULL on error
 *
 * Automatically creates appropriate neuron types and layer organization
 */
brain_region_t* brain_region_create(brain_region_type_t type, uint32_t num_neurons);

/**
 * @brief Destroy brain region
 *
 * @param region Brain region (NULL safe)
 */
void brain_region_destroy(brain_region_t* region);

/**
 * @brief Organize region into minicolumns
 *
 * @param region Brain region
 * @param columns_x Number of minicolumns in X dimension
 * @param columns_y Number of minicolumns in Y dimension
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t brain_region_organize_columns(brain_region_t* region,
                                               uint32_t columns_x,
                                               uint32_t columns_y);

/**
 * @brief Get neuron IDs in a specific layer
 *
 * @param region Brain region
 * @param layer Cortical layer
 * @param out_neuron_ids Output array (pre-allocated)
 * @param max_neurons Maximum neurons to return
 * @return Number of neurons returned
 */
uint32_t brain_region_get_layer_neurons(brain_region_t* region,
                                         cortical_layer_t layer,
                                         uint32_t* out_neuron_ids,
                                         uint32_t max_neurons);

// ============================================================================
// INTER-REGION CONNECTIVITY
// ============================================================================

/**
 * @brief Connect two brain regions
 *
 * @param brain Brain module
 * @param source_region_id Source region ID
 * @param target_region_id Target region ID
 * @param connection_density Density of connections (0-1)
 * @return NIMCP_SUCCESS on success
 *
 * Creates biologically realistic connections:
 * - Feedforward: Layer 2/3 → Layer 4
 * - Feedback: Layer 6 → Layer 1
 * - Lateral: within layers
 */
nimcp_result_t brain_module_connect_regions(brain_module_t* brain,
                                             uint32_t source_region_id,
                                             uint32_t target_region_id,
                                             float connection_density);

/**
 * @brief Connect specific layers between regions
 *
 * @param brain Brain module
 * @param source_region_id Source region
 * @param source_layer Source layer
 * @param target_region_id Target region
 * @param target_layer Target layer
 * @param connection_density Connection density (0-1)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t brain_module_connect_layers(brain_module_t* brain,
                                            uint32_t source_region_id,
                                            cortical_layer_t source_layer,
                                            uint32_t target_region_id,
                                            cortical_layer_t target_layer,
                                            float connection_density);

// ============================================================================
// SENSORY INPUT & PROCESSING
// ============================================================================

/**
 * @brief Process sensory input in a region
 *
 * @param region Brain region
 * @param input Input values
 * @param input_size Number of input values
 * @param timestamp Current timestamp (µs)
 * @return NIMCP_SUCCESS on success
 *
 * Distributes input to Layer 4 neurons (sensory input layer)
 */
nimcp_result_t brain_region_process_input(brain_region_t* region,
                                           const float* input,
                                           uint32_t input_size,
                                           uint64_t timestamp);

/**
 * @brief Get region output
 *
 * @param region Brain region
 * @param output Output buffer (pre-allocated)
 * @param output_size Maximum output size
 * @return Number of output values returned
 *
 * Collects activity from Layer 5 neurons (output layer)
 */
uint32_t brain_region_get_output(brain_region_t* region,
                                  float* output,
                                  uint32_t output_size);

// ============================================================================
// SIMULATION & STEPPING
// ============================================================================

/**
 * @brief Step brain module forward in time
 *
 * @param brain Brain module
 * @param delta_t Time step (µs)
 * @return NIMCP_SUCCESS on success
 *
 * Steps all regions and propagates signals between regions
 */
nimcp_result_t brain_module_step(brain_module_t* brain, uint64_t delta_t);

/**
 * @brief Step individual region forward in time
 *
 * @param region Brain region
 * @param delta_t Time step (µs)
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t brain_region_step(brain_region_t* region, uint64_t delta_t);

// ============================================================================
// STATISTICS & MONITORING
// ============================================================================

/**
 * @brief Region statistics
 */
typedef struct {
    uint32_t total_neurons;
    uint32_t active_neurons;         /**< Currently spiking */
    uint32_t num_minicolumns;
    float avg_activity;              /**< Average firing rate */
    float layer_activity[LAYER_COUNT]; /**< Activity per layer */
} brain_region_stats_t;

/**
 * @brief Get region statistics
 *
 * @param region Brain region
 * @param stats Output statistics
 * @return NIMCP_SUCCESS on success
 */
nimcp_result_t brain_region_get_stats(brain_region_t* region,
                                       brain_region_stats_t* stats);

/**
 * @brief Get human-readable region name
 *
 * @param type Region type
 * @return Region name string
 */
const char* brain_region_get_name(brain_region_type_t type);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_REGIONS_H
