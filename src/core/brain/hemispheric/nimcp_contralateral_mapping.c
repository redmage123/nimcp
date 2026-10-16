//=============================================================================
// nimcp_contralateral_mapping.c - Contralateral Motor/Sensory Mapping
//=============================================================================
/**
 * @file nimcp_contralateral_mapping.c
 * @brief Implementation of cross-body neural mapping
 */

#include "core/brain/hemispheric/nimcp_contralateral_mapping.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(contralateral_mapping, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Homunculus Cortical Representation Fractions
//=============================================================================

/**
 * Motor cortex fractions (based on Penfield's homunculus)
 * Hand, face, and tongue have disproportionately large representations
 */
static const float MOTOR_CORTEX_FRACTIONS[BODY_REGION_COUNT] = {
    0.04f,  // FOOT
    0.06f,  // LEG
    0.05f,  // TRUNK
    0.04f,  // SHOULDER
    0.08f,  // ARM
    0.15f,  // HAND - large representation
    0.12f,  // FINGERS - fine control
    0.03f,  // NECK
    0.10f,  // FACE
    0.12f,  // LIPS - speech
    0.15f,  // TONGUE - speech, taste
    0.06f   // THROAT
};

/**
 * Sensory cortex fractions
 * Similar to motor but with slight differences
 */
static const float SENSORY_CORTEX_FRACTIONS[BODY_REGION_COUNT] = {
    0.05f,  // FOOT
    0.07f,  // LEG
    0.05f,  // TRUNK
    0.04f,  // SHOULDER
    0.08f,  // ARM
    0.18f,  // HAND - high sensitivity
    0.14f,  // FINGERS - very high sensitivity
    0.03f,  // NECK
    0.10f,  // FACE
    0.10f,  // LIPS
    0.10f,  // TONGUE
    0.06f   // THROAT
};

//=============================================================================
// Internal Helpers
//=============================================================================

/**
 * @brief Initialize somatotopic regions with homunculus weighting
 */
static void init_somatotopic_regions(
    somatotopic_region_t* regions,
    const float* cortex_fractions,
    uint32_t total_neurons,
    bool is_motor
) {
    if (!regions || !cortex_fractions) return;

    uint32_t neuron_idx = 0;

    for (int i = 0; i < BODY_REGION_COUNT; i++) {
        regions[i].region = (body_region_t)i;
        regions[i].cortex_fraction = cortex_fractions[i];

        // Allocate neurons proportionally
        uint32_t region_neurons = (uint32_t)(cortex_fractions[i] * total_neurons);
        if (region_neurons < 1) region_neurons = 1;

        regions[i].neuron_start_idx = neuron_idx;
        regions[i].neuron_count = region_neurons;
        neuron_idx += region_neurons;

        // Set sensitivity/precision based on cortex fraction
        if (is_motor) {
            regions[i].motor_precision = cortex_fractions[i] * 2.0f;  // Higher fraction = finer control
            regions[i].sensitivity = 0.5f;
        } else {
            regions[i].sensitivity = cortex_fractions[i] * 2.0f;  // Higher fraction = more sensitive
            regions[i].motor_precision = 0.5f;
        }
    }
}

/**
 * @brief Get neuron index range for body region
 */
static void get_region_neurons(
    const somatotopic_region_t* regions,
    body_region_t region,
    uint32_t* start,
    uint32_t* count
) {
    if (!regions || !start || !count) return;

    if (region >= BODY_REGION_COUNT) {
        *start = 0;
        *count = 0;
        return;
    }

    *start = regions[region].neuron_start_idx;
    *count = regions[region].neuron_count;
}

//=============================================================================
// Lifecycle API
//=============================================================================

contralateral_config_t contralateral_default_config(void) {
    contralateral_config_t config = {
        .crossing_fraction = CONTRALATERAL_CROSSING_FRACTION,
        .enable_somatotopy = true,
        .enable_homunculus_weighting = true,
        .total_motor_neurons = 256,
        .total_sensory_neurons = 256
    };
    return config;
}

contralateral_system_t* contralateral_create(
    const contralateral_config_t* config,
    hemisphere_id_t hemisphere_id
) {
    contralateral_system_t* system = nimcp_malloc(sizeof(contralateral_system_t));
    if (!system) {
        NIMCP_LOGGING_ERROR("contralateral_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "system is NULL");

        return NULL;
    }
    memset(system, 0, sizeof(contralateral_system_t));

    // Apply configuration
    if (config) {
        system->config = *config;
    } else {
        system->config = contralateral_default_config();
    }

    // Set hemisphere association
    system->hemisphere_id = hemisphere_id;

    // Left hemisphere controls RIGHT body side (contralateral)
    system->controlled_side = (hemisphere_id == HEMISPHERE_LEFT)
        ? BODY_SIDE_RIGHT
        : BODY_SIDE_LEFT;

    // Initialize somatotopic regions
    init_somatotopic_regions(
        system->motor_regions,
        MOTOR_CORTEX_FRACTIONS,
        system->config.total_motor_neurons,
        true
    );

    init_somatotopic_regions(
        system->sensory_regions,
        SENSORY_CORTEX_FRACTIONS,
        system->config.total_sensory_neurons,
        false
    );

    // Allocate neuron activation arrays
    system->motor_neuron_activations = nimcp_malloc(
        system->config.total_motor_neurons * sizeof(float)
    );
    if (!system->motor_neuron_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "contralateral_create: failed to allocate motor_neuron_activations");
        nimcp_free(system);
        return NULL;
    }
    memset(system->motor_neuron_activations, 0,
           system->config.total_motor_neurons * sizeof(float));

    system->sensory_neuron_activations = nimcp_malloc(
        system->config.total_sensory_neurons * sizeof(float)
    );
    if (!system->sensory_neuron_activations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "contralateral_create: failed to allocate sensory_neuron_activations");
        nimcp_free(system->motor_neuron_activations);
        nimcp_free(system);
        return NULL;
    }
    memset(system->sensory_neuron_activations, 0,
           system->config.total_sensory_neurons * sizeof(float));

    system->initialized = true;

    NIMCP_LOGGING_INFO("Created contralateral mapping for %s hemisphere",
        hemisphere_id == HEMISPHERE_LEFT ? "left" : "right");

    return system;
}

void contralateral_destroy(contralateral_system_t* system) {
    if (!system) return;

    if (system->motor_neuron_activations) {
        nimcp_free(system->motor_neuron_activations);
    }
    if (system->sensory_neuron_activations) {
        nimcp_free(system->sensory_neuron_activations);
    }

    nimcp_free(system);
}

//=============================================================================
// Motor Mapping API
//=============================================================================

int contralateral_process_motor(
    contralateral_system_t* system,
    const float* hemisphere_output,
    uint32_t num_outputs,
    motor_command_t* body_commands,
    uint32_t max_commands
) {
    if (!system || !system->initialized || !hemisphere_output || !body_commands) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "contralateral_process_motor: NULL or uninitialized parameter");
        return -1;
    }

    // Map hemisphere outputs to motor neuron activations
    uint32_t copy_count = (num_outputs < system->config.total_motor_neurons)
        ? num_outputs : system->config.total_motor_neurons;

    memcpy(system->motor_neuron_activations, hemisphere_output,
           copy_count * sizeof(float));

    // Generate motor commands for each body region
    uint32_t command_count = 0;

    for (int i = 0; i < BODY_REGION_COUNT && command_count < max_commands; i++) {
        uint32_t start, count;
        get_region_neurons(system->motor_regions, (body_region_t)i, &start, &count);

        // Average activation for this region
        float total_activation = 0.0f;
        for (uint32_t j = start; j < start + count && j < copy_count; j++) {
            total_activation += system->motor_neuron_activations[j];
        }
        float avg_activation = (count > 0) ? total_activation / count : 0.0f;

        // Only generate command if activation is significant
        if (avg_activation > 0.1f) {
            body_commands[command_count].region = (body_region_t)i;
            body_commands[command_count].side = system->controlled_side;
            body_commands[command_count].activation = avg_activation;
            body_commands[command_count].velocity = avg_activation * 0.5f;
            body_commands[command_count].force = avg_activation *
                system->motor_regions[i].motor_precision;

            command_count++;
        }
    }

    system->motor_commands_processed++;

    return (int)command_count;
}

float contralateral_get_motor_activation(
    const contralateral_system_t* system,
    body_region_t region
) {
    if (!system || !system->initialized || region >= BODY_REGION_COUNT) {
        return 0.0f;
    }

    uint32_t start, count;
    get_region_neurons(system->motor_regions, region, &start, &count);

    if (count == 0) return 0.0f;

    float total = 0.0f;
    for (uint32_t i = start; i < start + count; i++) {
        if (i < system->config.total_motor_neurons) {
            total += system->motor_neuron_activations[i];
        }
    }

    return total / count;
}

//=============================================================================
// Sensory Mapping API
//=============================================================================

int contralateral_process_sensory(
    contralateral_system_t* system,
    const sensory_input_t* body_inputs,
    uint32_t num_inputs,
    float* hemisphere_input,
    uint32_t max_activations
) {
    if (!system || !system->initialized || !body_inputs || !hemisphere_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "contralateral_process_sensory: NULL or uninitialized parameter");
        return -1;
    }

    // Clear current sensory activations
    memset(system->sensory_neuron_activations, 0,
           system->config.total_sensory_neurons * sizeof(float));

    // Process each sensory input
    for (uint32_t i = 0; i < num_inputs; i++) {
        const sensory_input_t* input = &body_inputs[i];

        // Only process inputs from the side this hemisphere receives from
        // (which is the OPPOSITE of the side it controls)
        body_side_t receives_from = (system->controlled_side == BODY_SIDE_RIGHT)
            ? BODY_SIDE_RIGHT : BODY_SIDE_LEFT;

        // Apply crossing fraction
        float crossing_weight = system->config.crossing_fraction;
        if (input->side != receives_from && input->side != BODY_SIDE_MIDLINE) {
            crossing_weight = 1.0f - system->config.crossing_fraction;  // Ipsilateral
        }

        if (crossing_weight < 0.01f) continue;

        // Get neurons for this body region
        uint32_t start, count;
        get_region_neurons(system->sensory_regions, input->region, &start, &count);

        // Compute combined sensory intensity
        float intensity = input->intensity * 0.4f +
                          input->pressure * 0.3f +
                          input->proprioception * 0.2f +
                          input->pain * 0.1f;

        // Apply sensitivity weighting
        intensity *= system->sensory_regions[input->region].sensitivity;
        intensity *= crossing_weight;

        // Activate neurons for this region
        for (uint32_t j = start; j < start + count; j++) {
            if (j < system->config.total_sensory_neurons) {
                system->sensory_neuron_activations[j] += intensity;
            }
        }
    }

    // Clamp activations and copy to output
    uint32_t output_count = (system->config.total_sensory_neurons < max_activations)
        ? system->config.total_sensory_neurons : max_activations;

    for (uint32_t i = 0; i < output_count; i++) {
        hemisphere_input[i] = fminf(1.0f, system->sensory_neuron_activations[i]);
    }

    system->sensory_inputs_processed++;

    return (int)output_count;
}

float contralateral_get_sensory_activation(
    const contralateral_system_t* system,
    body_region_t region
) {
    if (!system || !system->initialized || region >= BODY_REGION_COUNT) {
        return 0.0f;
    }

    uint32_t start, count;
    get_region_neurons(system->sensory_regions, region, &start, &count);

    if (count == 0) return 0.0f;

    float total = 0.0f;
    for (uint32_t i = start; i < start + count; i++) {
        if (i < system->config.total_sensory_neurons) {
            total += system->sensory_neuron_activations[i];
        }
    }

    return total / count;
}

//=============================================================================
// Utility API
//=============================================================================

bool contralateral_controls_side(
    const contralateral_system_t* system,
    body_side_t side
) {
    if (!system || !system->initialized) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "contralateral_controls_side: required parameter is NULL (system, system->initialized)");
        return false;
    }
    return system->controlled_side == side;
}

body_side_t contralateral_get_controlled_side(
    const contralateral_system_t* system
) {
    if (!system || !system->initialized) {
        return BODY_SIDE_LEFT;
    }
    return system->controlled_side;
}

float contralateral_get_cortex_fraction(
    const contralateral_system_t* system,
    body_region_t region,
    bool is_motor
) {
    if (!system || !system->initialized || region >= BODY_REGION_COUNT) {
        return 0.0f;
    }

    if (is_motor) {
        return system->motor_regions[region].cortex_fraction;
    } else {
        return system->sensory_regions[region].cortex_fraction;
    }
}

const char* contralateral_region_to_string(body_region_t region) {
    static const char* names[BODY_REGION_COUNT] = {
        "Foot",
        "Leg",
        "Trunk",
        "Shoulder",
        "Arm",
        "Hand",
        "Fingers",
        "Neck",
        "Face",
        "Lips",
        "Tongue",
        "Throat"
    };

    if (region >= BODY_REGION_COUNT) {
        return "Unknown";
    }

    return names[region];
}
