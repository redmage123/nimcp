//=============================================================================
// nimcp_contralateral_mapping.h - Contralateral Motor/Sensory Mapping
//=============================================================================
/**
 * @file nimcp_contralateral_mapping.h
 * @brief Cross-body neural mapping between hemispheres and body sides
 *
 * WHAT: Topographic mapping of body regions to hemisphere neurons
 * WHY:  Left hemisphere controls right body, right hemisphere controls left body
 * HOW:  Somatotopic organization (motor/sensory homunculus), signal crossing
 *
 * BIOLOGICAL BASIS:
 * ==================================================================================
 *
 * CONTRALATERAL ORGANIZATION:
 * ---------------------------
 * The brain's motor and sensory systems exhibit contralateral organization:
 * - Motor cortex of left hemisphere → right side muscles
 * - Motor cortex of right hemisphere → left side muscles
 * - Sensory input from right body → left somatosensory cortex
 * - Sensory input from left body → right somatosensory cortex
 *
 * CROSSING LOCATIONS:
 * -------------------
 * - Corticospinal tract: Decussation at medullary pyramids (~90%)
 * - Sensory pathways: Cross at spinal cord (spinothalamic) or medulla (dorsal column)
 * - Some fibers remain ipsilateral (~10% for motor)
 *
 * SOMATOTOPIC ORGANIZATION (HOMUNCULUS):
 * --------------------------------------
 * Body parts are mapped topographically in motor/sensory cortex:
 * - Medial: Lower limbs, trunk
 * - Lateral: Upper limbs, hands
 * - Most lateral: Face, tongue
 * - Hand/face regions are disproportionately large (fine motor control)
 *
 * ARCHITECTURE:
 * ```
 * +=========================================================================+
 * |                    CONTRALATERAL MAPPING                                |
 * +=========================================================================+
 * |                                                                          |
 * |   LEFT HEMISPHERE                        RIGHT HEMISPHERE               |
 * |   ┌─────────────────┐                   ┌─────────────────┐            |
 * |   │  Motor Cortex   │                   │  Motor Cortex   │            |
 * |   │  ┌───────────┐  │                   │  ┌───────────┐  │            |
 * |   │  │ Leg │Face │  │                   │  │ Face│ Leg │  │            |
 * |   │  │ Arm │Mouth│  │                   │  │Mouth│ Arm │  │            |
 * |   │  │ Hand│Tongue│ │                   │  │Tongue│Hand│  │            |
 * |   │  └───────────┘  │                   │  └───────────┘  │            |
 * |   └────────┬────────┘                   └────────┬────────┘            |
 * |            │                                     │                      |
 * |            │         ┌───────────────┐          │                      |
 * |            └────────>│  DECUSSATION  │<─────────┘                      |
 * |                      │  (Crossing)   │                                  |
 * |                      └───────┬───────┘                                  |
 * |                              │                                          |
 * |            ┌─────────────────┴─────────────────┐                       |
 * |            │                                   │                        |
 * |            v                                   v                        |
 * |   ┌─────────────────┐                 ┌─────────────────┐              |
 * |   │   RIGHT BODY    │                 │    LEFT BODY    │              |
 * |   │   - Right arm   │                 │   - Left arm    │              |
 * |   │   - Right leg   │                 │   - Left leg    │              |
 * |   │   - Right face  │                 │   - Left face   │              |
 * |   └─────────────────┘                 └─────────────────┘              |
 * |                                                                          |
 * +=========================================================================+
 * ```
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_CONTRALATERAL_MAPPING_H
#define NIMCP_CONTRALATERAL_MAPPING_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Constants
//=============================================================================

/** Number of body regions in somatotopic map */
#define BODY_REGION_COUNT 12

/** Default crossing fraction (90% contralateral) */
#define CONTRALATERAL_CROSSING_FRACTION 0.90f

/** Ipsilateral fraction (10% same-side) */
#define IPSILATERAL_FRACTION 0.10f

//=============================================================================
// Types
//=============================================================================

/**
 * @brief Body regions for somatotopic mapping
 *
 * Ordered medial to lateral in motor cortex
 */
typedef enum {
    BODY_REGION_FOOT,
    BODY_REGION_LEG,
    BODY_REGION_TRUNK,
    BODY_REGION_SHOULDER,
    BODY_REGION_ARM,
    BODY_REGION_HAND,
    BODY_REGION_FINGERS,
    BODY_REGION_NECK,
    BODY_REGION_FACE,
    BODY_REGION_LIPS,
    BODY_REGION_TONGUE,
    BODY_REGION_THROAT
} body_region_t;

/**
 * @brief Body side for lateralized processing
 */
typedef enum {
    BODY_SIDE_LEFT,
    BODY_SIDE_RIGHT,
    BODY_SIDE_MIDLINE    /**< Trunk, some facial features */
} body_side_t;

/**
 * @brief Somatotopic region representation in cortex
 */
typedef struct {
    body_region_t region;
    float cortex_fraction;           /**< Fraction of cortex dedicated (homunculus) */
    uint32_t neuron_start_idx;       /**< Start index in neuron array */
    uint32_t neuron_count;           /**< Number of neurons for this region */
    float sensitivity;               /**< Sensory sensitivity factor */
    float motor_precision;           /**< Motor control precision */
} somatotopic_region_t;

/**
 * @brief Motor command for a body region
 */
typedef struct {
    body_region_t region;
    body_side_t side;
    float activation;                /**< Motor activation (0.0-1.0) */
    float velocity;                  /**< Target movement velocity */
    float force;                     /**< Target force */
} motor_command_t;

/**
 * @brief Sensory input from a body region
 */
typedef struct {
    body_region_t region;
    body_side_t side;
    float intensity;                 /**< Sensory intensity (0.0-1.0) */
    float temperature;               /**< Temperature sensation */
    float pressure;                  /**< Pressure/touch sensation */
    float pain;                      /**< Pain signal (0.0-1.0) */
    float proprioception;            /**< Position sense */
} sensory_input_t;

/**
 * @brief Contralateral mapping configuration
 */
typedef struct {
    float crossing_fraction;         /**< Fraction crossing to opposite side */
    bool enable_somatotopy;          /**< Use somatotopic organization */
    bool enable_homunculus_weighting; /**< Weight by cortical representation */
    uint32_t total_motor_neurons;    /**< Total motor neuron count */
    uint32_t total_sensory_neurons;  /**< Total sensory neuron count */
} contralateral_config_t;

/**
 * @brief Contralateral mapping system
 */
typedef struct {
    // Configuration
    contralateral_config_t config;

    // Somatotopic maps
    somatotopic_region_t motor_regions[BODY_REGION_COUNT];
    somatotopic_region_t sensory_regions[BODY_REGION_COUNT];

    // Neuron pools
    float* motor_neuron_activations;
    float* sensory_neuron_activations;

    // Hemisphere association
    hemisphere_id_t hemisphere_id;
    body_side_t controlled_side;     /**< Which body side this hemisphere controls */

    // Statistics
    uint64_t motor_commands_processed;
    uint64_t sensory_inputs_processed;

    bool initialized;
} contralateral_system_t;

//=============================================================================
// Lifecycle API
//=============================================================================

/**
 * @brief Get default contralateral configuration
 */
contralateral_config_t contralateral_default_config(void);

/**
 * @brief Create contralateral mapping system
 *
 * @param config Configuration
 * @param hemisphere_id Which hemisphere this maps for
 * @return System instance or NULL on failure
 */
contralateral_system_t* contralateral_create(
    const contralateral_config_t* config,
    hemisphere_id_t hemisphere_id
);

/**
 * @brief Destroy contralateral mapping system
 */
void contralateral_destroy(contralateral_system_t* system);

//=============================================================================
// Motor Mapping API
//=============================================================================

/**
 * @brief Process motor command from hemisphere to body
 *
 * WHAT: Convert hemisphere motor output to body-side commands
 * WHY:  Apply contralateral crossing and somatotopic mapping
 *
 * @param system Contralateral system
 * @param hemisphere_output Raw motor output from hemisphere
 * @param num_outputs Number of outputs
 * @param body_commands Output motor commands for body
 * @param max_commands Maximum commands to output
 * @return Number of commands generated, negative on error
 */
int contralateral_process_motor(
    contralateral_system_t* system,
    const float* hemisphere_output,
    uint32_t num_outputs,
    motor_command_t* body_commands,
    uint32_t max_commands
);

/**
 * @brief Get motor activation for specific body region
 *
 * @param system Contralateral system
 * @param region Target body region
 * @return Activation level (0.0-1.0)
 */
float contralateral_get_motor_activation(
    const contralateral_system_t* system,
    body_region_t region
);

//=============================================================================
// Sensory Mapping API
//=============================================================================

/**
 * @brief Process sensory input from body to hemisphere
 *
 * WHAT: Convert body-side sensory input to hemisphere activations
 * WHY:  Apply contralateral crossing and somatotopic mapping
 *
 * @param system Contralateral system
 * @param body_inputs Sensory inputs from body
 * @param num_inputs Number of inputs
 * @param hemisphere_input Output activations for hemisphere
 * @param max_activations Maximum activations to output
 * @return Number of activations generated, negative on error
 */
int contralateral_process_sensory(
    contralateral_system_t* system,
    const sensory_input_t* body_inputs,
    uint32_t num_inputs,
    float* hemisphere_input,
    uint32_t max_activations
);

/**
 * @brief Get sensory activation for specific body region
 *
 * @param system Contralateral system
 * @param region Source body region
 * @return Activation level (0.0-1.0)
 */
float contralateral_get_sensory_activation(
    const contralateral_system_t* system,
    body_region_t region
);

//=============================================================================
// Utility API
//=============================================================================

/**
 * @brief Check if this hemisphere controls given body side
 */
bool contralateral_controls_side(
    const contralateral_system_t* system,
    body_side_t side
);

/**
 * @brief Get the body side this hemisphere controls
 */
body_side_t contralateral_get_controlled_side(
    const contralateral_system_t* system
);

/**
 * @brief Get cortical representation size for body region (homunculus)
 */
float contralateral_get_cortex_fraction(
    const contralateral_system_t* system,
    body_region_t region,
    bool is_motor
);

/**
 * @brief Get string name for body region
 */
const char* contralateral_region_to_string(body_region_t region);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_CONTRALATERAL_MAPPING_H
