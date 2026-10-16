//=============================================================================
// nimcp_brain_hemisphere.h - Single Brain Hemisphere
//=============================================================================
/**
 * @file nimcp_brain_hemisphere.h
 * @brief Single hemisphere structure wrapping brain_t
 *
 * WHAT: Encapsulates one hemisphere of a bilateral brain
 * WHY:  Enables independent processing, learning, and resource management
 * HOW:  Wraps existing brain_t with hemisphere-specific metadata
 *
 * BIOLOGICAL BASIS:
 * - Each hemisphere has ~43 billion neurons (half of brain)
 * - Independent neural networks with specialized regions
 * - Contralateral motor/sensory mapping
 * - Local neuromodulator concentrations
 * - Hemisphere-specific glial systems
 *
 * DESIGN PRINCIPLE:
 * Wrapper approach - each hemisphere contains a complete brain_t instance,
 * allowing full use of existing brain APIs while adding hemispheric features.
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 * @version 1.0.0
 */

#ifndef NIMCP_BRAIN_HEMISPHERE_H
#define NIMCP_BRAIN_HEMISPHERE_H

#include <stdint.h>
#include <stdbool.h>
#include "core/brain/nimcp_brain.h"
#include "core/brain/hemispheric/nimcp_lateralization.h"
#include "plasticity/neuromodulators/nimcp_neuromodulators.h"
#include "glial/integration/nimcp_glial_integration.h"
#include "utils/platform/nimcp_platform_tier.h"
#include "async/nimcp_bio_async.h"

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Forward Declarations
//=============================================================================

typedef struct brain_hemisphere_struct brain_hemisphere_t;
typedef struct corpus_callosum_struct corpus_callosum_t;

//=============================================================================
// Configuration
//=============================================================================

/**
 * @brief Configuration for a single hemisphere
 */
typedef struct {
    // Brain configuration (passed to brain_create)
    const char* task_name;
    brain_size_t size;
    brain_task_t task;
    uint32_t num_inputs;
    uint32_t num_outputs;

    // Hemisphere-specific
    hemisphere_id_t hemisphere_id;
    float specialization_weights[COGNITIVE_DOMAIN_COUNT];

    // Neuromodulator configuration
    bool enable_local_neuromod;          // Hemisphere-local pool
    float neuromod_diffusion_rate;       // Cross-hemisphere diffusion

    // Glial configuration
    bool enable_local_glial;

    // Resource management
    platform_tier_t initial_tier;

    // Bio-async
    bool enable_bio_async;
} hemisphere_config_t;

/**
 * @brief Statistics for hemisphere operations
 */
typedef struct {
    // Processing statistics
    uint64_t total_updates;
    uint64_t total_inferences;
    uint64_t total_learning_steps;

    // Activity
    float avg_activity_level;
    float peak_activity_level;
    float current_activity;

    // Resource usage
    float avg_energy_consumption;
    uint64_t tier_switches;

    // Communication
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t callosum_transfers;

    // Timing
    float avg_update_time_ms;
    float avg_inference_time_ms;
} hemisphere_stats_t;

//=============================================================================
// Contralateral Mapping
//=============================================================================

/**
 * @brief Contralateral body mapping
 *
 * BIOLOGICAL BASIS:
 * - Left hemisphere controls right body side
 * - Right hemisphere controls left body side
 * - Sensory input crosses at brainstem/spinal cord
 */
typedef struct {
    // Motor output mapping
    uint32_t* motor_neuron_ids;          // Neurons controlling contralateral side
    uint32_t num_motor_neurons;

    // Sensory input mapping
    uint32_t* sensory_neuron_ids;        // Neurons receiving contralateral input
    uint32_t num_sensory_neurons;

    // Body side
    bool controls_right_side;            // true for left hemisphere
} contralateral_map_t;

//=============================================================================
// Main Structure
//=============================================================================

/**
 * @brief Single brain hemisphere
 *
 * WHAT: One hemisphere of a bilateral brain
 * WHY:  Independent processing with inter-hemispheric coordination
 * HOW:  Wraps brain_t with hemisphere-specific features
 */
struct brain_hemisphere_struct {
    // === Identity ===
    hemisphere_id_t id;                  // LEFT or RIGHT
    uint64_t creation_time;

    // === Core Brain Instance ===
    brain_t brain;                       // Underlying brain (existing API)

    // === Specialization ===
    float specialization[COGNITIVE_DOMAIN_COUNT];  // Domain weights

    // === Neuromodulators (hemisphere-local) ===
    neuromodulator_system_t neuromod;    // Local pool
    float neuromod_diffusion_rate;       // Cross-hemisphere diffusion

    // === Glial System ===
    glial_integration_t* glial;

    // === Contralateral Mapping ===
    contralateral_map_t motor_map;
    contralateral_map_t sensory_map;

    // === State ===
    float activity_level;                // Current activation [0.0-1.0]
    float energy_consumption;            // Current power draw
    bool is_active;                      // Processing enabled

    // === Resource Management ===
    platform_tier_t current_tier;

    // === Bio-Async ===
    bio_module_context_t bio_ctx;
    bool bio_async_enabled;

    // === Parent Reference ===
    corpus_callosum_t* callosum;         // Connection to other hemisphere

    // === Statistics ===
    hemisphere_stats_t stats;

    // === Thread Safety ===
    void* mutex;                         // nimcp_mutex_t*
};

//=============================================================================
// Lifecycle
//=============================================================================

/**
 * @brief Get default hemisphere configuration
 *
 * @param hemisphere_id Which hemisphere (LEFT or RIGHT)
 * @return Default configuration
 */
hemisphere_config_t hemisphere_default_config(hemisphere_id_t hemisphere_id);

/**
 * @brief Create a brain hemisphere
 *
 * WHAT: Allocate and initialize one hemisphere
 * WHY:  Building block for hemispheric brain
 * HOW:  Create brain_t internally with hemisphere metadata
 *
 * @param config Hemisphere configuration
 * @return Hemisphere instance or NULL on error
 */
brain_hemisphere_t* hemisphere_create(const hemisphere_config_t* config);

/**
 * @brief Destroy a hemisphere
 *
 * @param hemisphere Hemisphere to destroy
 */
void hemisphere_destroy(brain_hemisphere_t* hemisphere);

//=============================================================================
// Processing
//=============================================================================

/**
 * @brief Update hemisphere state
 *
 * WHAT: Process one simulation timestep
 * WHY:  Advance neural dynamics, neuromodulators, glial
 * HOW:  Update internal brain, then hemisphere-specific systems
 *
 * @param hemisphere Hemisphere to update
 * @param dt Time delta in seconds
 * @return 0 on success
 */
int hemisphere_update(brain_hemisphere_t* hemisphere, float dt);

/**
 * @brief Run inference on hemisphere
 *
 * @param hemisphere Hemisphere to use
 * @param input Input array
 * @param input_size Number of inputs
 * @param output Output array (pre-allocated)
 * @param output_size Number of outputs
 * @return 0 on success
 */
int hemisphere_infer(
    brain_hemisphere_t* hemisphere,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
);

/**
 * @brief Train hemisphere
 *
 * @param hemisphere Hemisphere to train
 * @param input Training input
 * @param target Training target
 * @param size Input/target size
 * @return Loss value
 */
float hemisphere_train(
    brain_hemisphere_t* hemisphere,
    const float* input,
    const float* target,
    uint32_t size
);

//=============================================================================
// State Query
//=============================================================================

/**
 * @brief Get underlying brain_t (for existing API compatibility)
 */
brain_t hemisphere_get_brain(brain_hemisphere_t* hemisphere);

/**
 * @brief Get current activity level
 */
float hemisphere_get_activity(const brain_hemisphere_t* hemisphere);

/**
 * @brief Get energy consumption
 */
float hemisphere_get_energy(const brain_hemisphere_t* hemisphere);

/**
 * @brief Get current tier
 */
platform_tier_t hemisphere_get_tier(const brain_hemisphere_t* hemisphere);

/**
 * @brief Get statistics
 */
int hemisphere_get_stats(
    const brain_hemisphere_t* hemisphere,
    hemisphere_stats_t* stats
);

/**
 * @brief Get specialization weight for domain
 */
float hemisphere_get_specialization(
    const brain_hemisphere_t* hemisphere,
    cognitive_domain_t domain
);

//=============================================================================
// Resource Management
//=============================================================================

/**
 * @brief Set hemisphere tier
 *
 * @param hemisphere Hemisphere
 * @param tier New tier
 * @return 0 on success
 */
int hemisphere_set_tier(
    brain_hemisphere_t* hemisphere,
    platform_tier_t tier
);

/**
 * @brief Enable/disable hemisphere processing
 *
 * @param hemisphere Hemisphere
 * @param active true to enable, false to suspend
 * @return 0 on success
 */
int hemisphere_set_active(
    brain_hemisphere_t* hemisphere,
    bool active
);

//=============================================================================
// Neuromodulator Control
//=============================================================================

/**
 * @brief Get local neuromodulator level
 */
float hemisphere_get_neuromod(
    const brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type
);

/**
 * @brief Set local neuromodulator level
 */
int hemisphere_set_neuromod(
    brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type,
    float level
);

/**
 * @brief Apply neuromodulator diffusion from other hemisphere
 *
 * @param hemisphere Target hemisphere
 * @param type Neuromodulator type
 * @param other_level Level in other hemisphere
 * @return Resulting level after diffusion
 */
float hemisphere_apply_neuromod_diffusion(
    brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type,
    float other_level
);

//=============================================================================
// Contralateral Mapping
//=============================================================================

/**
 * @brief Map motor output to contralateral body side
 *
 * @param hemisphere Source hemisphere
 * @param motor_commands Input motor commands
 * @param num_commands Number of commands
 * @param body_output Output mapped to body side
 * @return 0 on success
 */
int hemisphere_map_motor_output(
    brain_hemisphere_t* hemisphere,
    const float* motor_commands,
    uint32_t num_commands,
    float* body_output
);

/**
 * @brief Map sensory input from contralateral body side
 *
 * @param hemisphere Target hemisphere
 * @param body_input Input from body side
 * @param num_inputs Number of inputs
 * @param sensory_input Output for hemisphere
 * @return 0 on success
 */
int hemisphere_map_sensory_input(
    brain_hemisphere_t* hemisphere,
    const float* body_input,
    uint32_t num_inputs,
    float* sensory_input
);

//=============================================================================
// Callosum Integration
//=============================================================================

/**
 * @brief Set callosum reference (called by hemispheric_brain)
 */
void hemisphere_set_callosum(
    brain_hemisphere_t* hemisphere,
    corpus_callosum_t* callosum
);

/**
 * @brief Check if connected to callosum
 */
bool hemisphere_has_callosum(const brain_hemisphere_t* hemisphere);

//=============================================================================
// Bio-Async
//=============================================================================

/**
 * @brief Connect to bio-async router
 */
int hemisphere_connect_bio_async(brain_hemisphere_t* hemisphere);

/**
 * @brief Disconnect from bio-async router
 */
int hemisphere_disconnect_bio_async(brain_hemisphere_t* hemisphere);

/**
 * @brief Check bio-async connection
 */
bool hemisphere_is_bio_async_connected(const brain_hemisphere_t* hemisphere);

//=============================================================================
// Learning Rate Control (for immune bridge modulation)
//=============================================================================

/**
 * @brief Get current learning rate for hemisphere
 *
 * @param hemisphere Hemisphere
 * @return Current learning rate
 */
float brain_hemisphere_get_learning_rate(const brain_hemisphere_t* hemisphere);

/**
 * @brief Set learning rate for hemisphere
 *
 * @param hemisphere Hemisphere
 * @param lr New learning rate
 * @return 0 on success
 */
int brain_hemisphere_set_learning_rate(
    brain_hemisphere_t* hemisphere,
    float lr
);

#ifdef __cplusplus
}
#endif

#endif // NIMCP_BRAIN_HEMISPHERE_H
