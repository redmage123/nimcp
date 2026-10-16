//=============================================================================
// nimcp_hemispheric_brain.c - Bilateral Brain Implementation
//=============================================================================
/**
 * @file nimcp_hemispheric_brain.c
 * @brief Implementation of two-hemisphere brain architecture
 *
 * BIOLOGICAL BASIS:
 * - ~86 billion neurons split between hemispheres
 * - Lateralized processing (language left, spatial right)
 * - Corpus callosum enables inter-hemispheric transfer
 * - Contralateral motor/sensory mapping
 *
 * PROCESSING MODES:
 * - Lateralized: Route to dominant hemisphere for domain
 * - Parallel: Both hemispheres process simultaneously
 * - Competitive: Race, winner provides output
 * - Cooperative: Combine outputs based on specialization
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/hemispheric/nimcp_hemispheric_brain.h"
#include "api/nimcp_api_exception.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/time/nimcp_time.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(hemispheric_brain, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Configuration
//=============================================================================

hemispheric_brain_config_t hemispheric_brain_default_config(void) {
    hemispheric_brain_config_t config = {
        .task_name = "hemispheric_brain",
        .task = BRAIN_TASK_PATTERN_MATCHING,
        .size = BRAIN_SIZE_MEDIUM,
        .num_inputs = 64,
        .num_outputs = 32,

        .left_config = hemisphere_default_config(HEMISPHERE_LEFT),
        .right_config = hemisphere_default_config(HEMISPHERE_RIGHT),

        .lateralization = lateralization_default_profile(),
        .callosum_config = callosum_default_config(),

        .default_mode = HEMISPHERIC_MODE_COOPERATIVE,
        .cooperation_strategy = COOPERATION_WEIGHTED,

        .initial_tier = PLATFORM_TIER_MEDIUM,
        .asymmetric_resources = false,
        .left_resource_fraction = 0.5f,

        .enable_shared_thalamus = true,
        .enable_shared_immune = true,
        .enable_bio_async = true
    };
    return config;
}

bool hemispheric_brain_validate_config(const hemispheric_brain_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemispheric_brain_validate_config: config is NULL");
        return false;
    }

    if (config->left_resource_fraction < 0.0f ||
        config->left_resource_fraction > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_brain_validate_config: left_resource_fraction out of range [0.0, 1.0]");
        return false;
    }

    if (!lateralization_validate(&config->lateralization)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_brain_validate_config: lateralization_validate is NULL");
        return false;
    }

    if (!callosum_validate_config(&config->callosum_config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_brain_validate_config: callosum_validate_config is NULL");
        return false;
    }

    return true;
}

//=============================================================================
// Lifecycle
//=============================================================================

hemispheric_brain_t* hemispheric_brain_create(
    const hemispheric_brain_config_t* config
) {
    hemispheric_brain_config_t cfg = config
        ? *config : hemispheric_brain_default_config();

    if (!hemispheric_brain_validate_config(&cfg)) {
        NIMCP_LOGGING_ERROR("Invalid hemispheric brain configuration");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hemispheric_brain_create: invalid configuration");
        return NULL;
    }

    hemispheric_brain_t* brain = nimcp_calloc(1, sizeof(hemispheric_brain_t));
    if (!brain) {
        NIMCP_LOGGING_ERROR("Failed to allocate hemispheric brain");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemispheric_brain_create: failed to allocate brain");
        return NULL;
    }

    // Create left hemisphere
    cfg.left_config.task_name = "left_hemisphere";
    cfg.left_config.num_inputs = cfg.num_inputs;
    cfg.left_config.num_outputs = cfg.num_outputs;
    cfg.left_config.size = cfg.size;
    cfg.left_config.initial_tier = cfg.initial_tier;

    brain->left = hemisphere_create(&cfg.left_config);
    if (!brain->left) {
        NIMCP_LOGGING_ERROR("Failed to create left hemisphere");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_create: failed to create left hemisphere");
        nimcp_free(brain);
        return NULL;
    }

    // Create right hemisphere
    cfg.right_config.task_name = "right_hemisphere";
    cfg.right_config.num_inputs = cfg.num_inputs;
    cfg.right_config.num_outputs = cfg.num_outputs;
    cfg.right_config.size = cfg.size;
    cfg.right_config.initial_tier = cfg.initial_tier;

    brain->right = hemisphere_create(&cfg.right_config);
    if (!brain->right) {
        NIMCP_LOGGING_ERROR("Failed to create right hemisphere");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_create: failed to create right hemisphere");
        hemisphere_destroy(brain->left);
        nimcp_free(brain);
        return NULL;
    }

    // Create corpus callosum
    brain->callosum = callosum_create(&cfg.callosum_config);
    if (!brain->callosum) {
        NIMCP_LOGGING_ERROR("Failed to create corpus callosum");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_create: failed to create corpus callosum");
        hemisphere_destroy(brain->left);
        hemisphere_destroy(brain->right);
        nimcp_free(brain);
        return NULL;
    }

    // Connect hemispheres to callosum
    callosum_connect_hemispheres(brain->callosum, brain->left, brain->right);

    // Copy lateralization profile
    memcpy(&brain->lateralization, &cfg.lateralization,
           sizeof(lateralization_profile_t));

    // Processing configuration
    brain->default_mode = cfg.default_mode;
    brain->cooperation_strategy = cfg.cooperation_strategy;

    // Resource management
    brain->asymmetric_resources = cfg.asymmetric_resources;
    brain->left_resource_fraction = cfg.left_resource_fraction;

    // State
    brain->callosum_intact = true;
    brain->is_active = true;
    brain->creation_time = nimcp_time_get_us();
    brain->update_count = 0;

    // Determine initial dominant hemisphere (based on lateralization)
    float total_left = 0.0f, total_right = 0.0f;
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        float dom = lateralization_get_dominance(&brain->lateralization,
                                                 (cognitive_domain_t)d);
        total_left += dom;
        total_right += (1.0f - dom);
    }
    brain->dominant_hemisphere = (total_left >= total_right)
        ? HEMISPHERE_LEFT : HEMISPHERE_RIGHT;

    // Mutex
    brain->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!brain->mutex) {
        callosum_destroy(brain->callosum);
        hemisphere_destroy(brain->left);
        hemisphere_destroy(brain->right);
        nimcp_free(brain);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "hemispheric_brain_create: brain->mutex is NULL");
        return NULL;
    }
    nimcp_mutex_init(brain->mutex, NULL);

    // Bio-async
    brain->bio_async_enabled = false;
    if (cfg.enable_bio_async) {
        hemispheric_brain_connect_bio_async(brain);
    }

    NIMCP_LOGGING_INFO("Hemispheric brain created with %s mode",
                       hemispheric_mode_name(cfg.default_mode));

    return brain;
}

void hemispheric_brain_destroy(hemispheric_brain_t* brain) {
    if (!brain) {
        return;
    }

    // Disconnect bio-async
    if (brain->bio_async_enabled) {
        hemispheric_brain_disconnect_bio_async(brain);
    }

    // Destroy callosum
    if (brain->callosum) {
        callosum_destroy(brain->callosum);
    }

    // Destroy hemispheres
    if (brain->left) {
        hemisphere_destroy(brain->left);
    }
    if (brain->right) {
        hemisphere_destroy(brain->right);
    }

    // Destroy shared structures
    if (brain->thalamus) {
        brain_destroy(*brain->thalamus);
        nimcp_free(brain->thalamus);
    }

    // Destroy mutex
    if (brain->mutex) {
        nimcp_mutex_free(brain->mutex);
    }

    nimcp_free(brain);
}

//=============================================================================
// Update and Processing
//=============================================================================

int hemispheric_brain_update(hemispheric_brain_t* brain, float dt) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_update: brain is NULL");
        return -1;
    }
    if (!brain->is_active) {
        return 0;  /* Inactive brain - normal, skip update */
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_update: failed to acquire mutex");
        return -1;
    }

    uint64_t start_time = nimcp_time_get_us();

    // Update both hemispheres
    int result = hemisphere_update(brain->left, dt);
    if (result != 0) {
        nimcp_mutex_unlock(brain->mutex);
        return result;
    }

    result = hemisphere_update(brain->right, dt);
    if (result != 0) {
        nimcp_mutex_unlock(brain->mutex);
        return result;
    }

    // Process callosum (deliver pending messages)
    if (brain->callosum_intact) {
        callosum_process_queues(brain->callosum);

        // Apply neuromodulator diffusion across hemispheres
        for (int i = 0; i < NEUROMOD_COUNT; i++) {
            neuromodulator_type_t type = (neuromodulator_type_t)i;
            float left_level = hemisphere_get_neuromod(brain->left, type);
            float right_level = hemisphere_get_neuromod(brain->right, type);

            hemisphere_apply_neuromod_diffusion(brain->left, type, right_level);
            hemisphere_apply_neuromod_diffusion(brain->right, type, left_level);
        }
    }

    // Shared thalamus update would go here if thalamus were connected
    // (thalamus is optional and may be NULL)

    // Update statistics
    brain->update_count++;
    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    brain->stats.avg_update_time_ms =
        brain->stats.avg_update_time_ms * 0.99f +
        (elapsed_us / 1000.0f) * 0.01f;

    brain->stats.left_activity = hemisphere_get_activity(brain->left);
    brain->stats.right_activity = hemisphere_get_activity(brain->right);
    brain->stats.left_energy = hemisphere_get_energy(brain->left);
    brain->stats.right_energy = hemisphere_get_energy(brain->right);
    brain->stats.total_energy = brain->stats.left_energy + brain->stats.right_energy;

    nimcp_mutex_unlock(brain->mutex);
    return 0;
}

int hemispheric_brain_process_lateralized(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    cognitive_domain_t domain,
    float* output,
    uint32_t output_size
) {
    if (!brain || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_process_lateralized: NULL parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_process_lateralized: failed to acquire mutex");
        return -1;
    }

    // Determine dominant hemisphere
    hemisphere_id_t dominant = lateralization_get_dominant_hemisphere(
        &brain->lateralization, domain);

    // Apply usage-based plasticity
    if (brain->lateralization.enable_plasticity) {
        lateralization_apply_usage_plasticity(&brain->lateralization,
                                              domain, dominant);
    }

    // Route to dominant hemisphere
    brain_hemisphere_t* target = (dominant == HEMISPHERE_LEFT)
        ? brain->left : brain->right;

    int result = hemisphere_infer(target, input, input_size,
                                  output, output_size);

    brain->stats.lateralized_operations++;

    nimcp_mutex_unlock(brain->mutex);
    return result;
}

int hemispheric_brain_process_parallel(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* left_output,
    float* right_output,
    uint32_t output_size
) {
    if (!brain || !input || !left_output || !right_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_process_parallel: NULL parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_process_parallel: failed to acquire mutex");
        return -1;
    }

    // Process both hemispheres
    int result = hemisphere_infer(brain->left, input, input_size,
                                  left_output, output_size);
    if (result != 0) {
        nimcp_mutex_unlock(brain->mutex);
        return result;
    }

    result = hemisphere_infer(brain->right, input, input_size,
                              right_output, output_size);

    brain->stats.parallel_operations++;

    nimcp_mutex_unlock(brain->mutex);
    return result;
}

int hemispheric_brain_process_competitive(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size,
    hemisphere_id_t* winner
) {
    if (!brain || !input || !output || !winner) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_process_competitive: NULL parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_process_competitive: failed to acquire mutex");
        return -1;
    }

    // Allocate temporary buffers
    float* left_out = nimcp_malloc(output_size * sizeof(float));
    float* right_out = nimcp_malloc(output_size * sizeof(float));
    if (!left_out || !right_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemispheric_brain_process_competitive: failed to allocate buffers");
        if (left_out) nimcp_free(left_out);
        if (right_out) nimcp_free(right_out);
        nimcp_mutex_unlock(brain->mutex);
        return -1;
    }

    int ret = 0;

    // Process both hemispheres
    if (hemisphere_infer(brain->left, input, input_size, left_out, output_size) != 0) {
        NIMCP_LOGGING_ERROR("Left hemisphere inference failed");
        ret = -1;
        goto cleanup;
    }
    if (hemisphere_infer(brain->right, input, input_size, right_out, output_size) != 0) {
        NIMCP_LOGGING_ERROR("Right hemisphere inference failed");
        ret = -1;
        goto cleanup;
    }

    // Determine winner based on activation strength
    float left_strength = 0.0f, right_strength = 0.0f;
    for (uint32_t i = 0; i < output_size; i++) {
        left_strength += fabsf(left_out[i]);
        right_strength += fabsf(right_out[i]);
    }

    if (left_strength >= right_strength) {
        *winner = HEMISPHERE_LEFT;
        memcpy(output, left_out, output_size * sizeof(float));
        brain->stats.left_wins++;
    } else {
        *winner = HEMISPHERE_RIGHT;
        memcpy(output, right_out, output_size * sizeof(float));
        brain->stats.right_wins++;
    }

    brain->stats.competitive_operations++;

cleanup:
    nimcp_free(left_out);
    nimcp_free(right_out);
    nimcp_mutex_unlock(brain->mutex);
    return ret;
}

int hemispheric_brain_process_cooperative(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
) {
    if (!brain || !input || !output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_process_cooperative: NULL parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_process_cooperative: failed to acquire mutex");
        return -1;
    }

    // Allocate temporary buffers
    float* left_out = nimcp_malloc(output_size * sizeof(float));
    float* right_out = nimcp_malloc(output_size * sizeof(float));
    if (!left_out || !right_out) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemispheric_brain_process_cooperative: failed to allocate buffers");
        if (left_out) nimcp_free(left_out);
        if (right_out) nimcp_free(right_out);
        nimcp_mutex_unlock(brain->mutex);
        return -1;
    }

    int ret = 0;

    // Process both hemispheres
    if (hemisphere_infer(brain->left, input, input_size, left_out, output_size) != 0) {
        NIMCP_LOGGING_ERROR("Left hemisphere inference failed");
        ret = -1;
        goto cleanup;
    }
    if (hemisphere_infer(brain->right, input, input_size, right_out, output_size) != 0) {
        NIMCP_LOGGING_ERROR("Right hemisphere inference failed");
        ret = -1;
        goto cleanup;
    }

    // Combine based on cooperation strategy
    switch (brain->cooperation_strategy) {
        case COOPERATION_AVERAGE:
            for (uint32_t i = 0; i < output_size; i++) {
                output[i] = (left_out[i] + right_out[i]) / 2.0f;
            }
            break;

        case COOPERATION_WEIGHTED: {
            // Weight by overall specialization
            float left_weight = 0.0f, right_weight = 0.0f;
            for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
                left_weight += hemisphere_get_specialization(
                    brain->left, (cognitive_domain_t)d);
                right_weight += hemisphere_get_specialization(
                    brain->right, (cognitive_domain_t)d);
            }
            float total = left_weight + right_weight;
            if (total > 0.0f) {
                left_weight /= total;
                right_weight /= total;
            } else {
                left_weight = right_weight = 0.5f;
            }

            for (uint32_t i = 0; i < output_size; i++) {
                output[i] = left_out[i] * left_weight +
                           right_out[i] * right_weight;
            }
            break;
        }

        case COOPERATION_DOMINANT: {
            // Use dominant hemisphere primarily
            float dom_weight = 0.7f;
            float sub_weight = 0.3f;
            if (brain->dominant_hemisphere == HEMISPHERE_LEFT) {
                for (uint32_t i = 0; i < output_size; i++) {
                    output[i] = left_out[i] * dom_weight +
                               right_out[i] * sub_weight;
                }
            } else {
                for (uint32_t i = 0; i < output_size; i++) {
                    output[i] = right_out[i] * dom_weight +
                               left_out[i] * sub_weight;
                }
            }
            break;
        }

        case COOPERATION_ATTENTION_GATED: {
            // Gate by current activity levels
            float left_activity = hemisphere_get_activity(brain->left);
            float right_activity = hemisphere_get_activity(brain->right);
            float total = left_activity + right_activity;
            if (total > 0.0f) {
                float left_gate = left_activity / total;
                float right_gate = right_activity / total;
                for (uint32_t i = 0; i < output_size; i++) {
                    output[i] = left_out[i] * left_gate +
                               right_out[i] * right_gate;
                }
            } else {
                for (uint32_t i = 0; i < output_size; i++) {
                    output[i] = (left_out[i] + right_out[i]) / 2.0f;
                }
            }
            break;
        }
    }

    brain->stats.cooperative_operations++;

cleanup:
    nimcp_free(left_out);
    nimcp_free(right_out);
    nimcp_mutex_unlock(brain->mutex);
    return ret;
}

int hemispheric_brain_infer(
    hemispheric_brain_t* brain,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return -1;
    }

    switch (brain->default_mode) {
        case HEMISPHERIC_MODE_LATERALIZED:
            // Default to general domain
            return hemispheric_brain_process_lateralized(
                brain, input, input_size, COGNITIVE_DOMAIN_LOGICAL_REASONING,
                output, output_size);

        case HEMISPHERIC_MODE_PARALLEL: {
            // Allocate right output, use output for left
            float* right_out = nimcp_malloc(output_size * sizeof(float));
            if (!right_out) {
                NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
                    "hemispheric_brain_infer: failed to allocate right_out");
                return -1;
            }
            int result = hemispheric_brain_process_parallel(
                brain, input, input_size, output, right_out, output_size);
            // Average for final output
            for (uint32_t i = 0; i < output_size; i++) {
                output[i] = (output[i] + right_out[i]) / 2.0f;
            }
            nimcp_free(right_out);
            return result;
        }

        case HEMISPHERIC_MODE_COMPETITIVE: {
            hemisphere_id_t winner;
            return hemispheric_brain_process_competitive(
                brain, input, input_size, output, output_size, &winner);
        }

        case HEMISPHERIC_MODE_COOPERATIVE:
        default:
            return hemispheric_brain_process_cooperative(
                brain, input, input_size, output, output_size);
    }
}

float hemispheric_brain_train(
    hemispheric_brain_t* brain,
    const float* input,
    const float* target,
    uint32_t size
) {
    if (!brain || !input || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_train: NULL parameter");
        return -1.0f;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_train: failed to acquire mutex");
        return -1.0f;
    }

    // Train both hemispheres
    float left_loss = hemisphere_train(brain->left, input, target, size);
    float right_loss = hemisphere_train(brain->right, input, target, size);

    nimcp_mutex_unlock(brain->mutex);

    return (left_loss + right_loss) / 2.0f;
}

//=============================================================================
// Hemisphere Access
//=============================================================================

brain_hemisphere_t* hemispheric_brain_get_left(hemispheric_brain_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_left: brain is NULL");
        return NULL;
    }
    return brain->left;
}

brain_hemisphere_t* hemispheric_brain_get_right(hemispheric_brain_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_right: brain is NULL");
        return NULL;
    }
    return brain->right;
}

brain_hemisphere_t* hemispheric_brain_get_hemisphere(
    hemispheric_brain_t* brain,
    hemisphere_id_t id
) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }
    return (id == HEMISPHERE_LEFT) ? brain->left : brain->right;
}

brain_hemisphere_t* hemispheric_brain_get_dominant(hemispheric_brain_t* brain) {
    if (!brain) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "brain is NULL");

        return NULL;

    }
    return (brain->dominant_hemisphere == HEMISPHERE_LEFT)
        ? brain->left : brain->right;
}

brain_t hemispheric_brain_get_brain(
    hemispheric_brain_t* brain,
    hemisphere_id_t id
) {
    brain_hemisphere_t* hemi = hemispheric_brain_get_hemisphere(brain, id);
    return hemisphere_get_brain(hemi);
}

//=============================================================================
// Lateralization Control
//=============================================================================

hemisphere_id_t hemispheric_brain_get_dominant_for(
    const hemispheric_brain_t* brain,
    cognitive_domain_t domain
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_dominant_for: brain is NULL");
        return HEMISPHERE_LEFT;
    }
    return lateralization_get_dominant_hemisphere(&brain->lateralization, domain);
}

float hemispheric_brain_get_dominance(
    const hemispheric_brain_t* brain,
    cognitive_domain_t domain
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_dominance: brain is NULL");
        return 0.5f;
    }
    return lateralization_get_dominance(&brain->lateralization, domain);
}

int hemispheric_brain_shift_dominance(
    hemispheric_brain_t* brain,
    cognitive_domain_t domain,
    float shift
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_shift_dominance: brain is NULL");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_shift_dominance: failed to acquire mutex");
        return -1;
    }

    int result = lateralization_shift_dominance(&brain->lateralization,
                                                domain, shift);
    nimcp_mutex_unlock(brain->mutex);
    return result;
}

int hemispheric_brain_set_lateralization(
    hemispheric_brain_t* brain,
    const lateralization_profile_t* profile
) {
    if (!brain || !profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_lateralization: NULL parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_set_lateralization: failed to acquire mutex");
        return -1;
    }

    memcpy(&brain->lateralization, profile, sizeof(lateralization_profile_t));
    nimcp_mutex_unlock(brain->mutex);
    return 0;
}

int hemispheric_brain_get_lateralization(
    const hemispheric_brain_t* brain,
    lateralization_profile_t* profile
) {
    if (!brain || !profile) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_lateralization: NULL parameter");
        return -1;
    }
    memcpy(profile, &brain->lateralization, sizeof(lateralization_profile_t));
    return 0;
}

//=============================================================================
// Callosum Control
//=============================================================================

corpus_callosum_t* hemispheric_brain_get_callosum(hemispheric_brain_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_callosum: brain is NULL");
        return NULL;
    }
    return brain->callosum;
}

int hemispheric_brain_disconnect_callosum(hemispheric_brain_t* brain) {
    if (!brain || !brain->callosum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_disconnect_callosum: brain or callosum is NULL");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_disconnect_callosum: failed to acquire mutex");
        return -1;
    }

    callosum_disconnect(brain->callosum);
    brain->callosum_intact = false;
    brain->stats.callosum_stats.disconnection_events++;
    nimcp_mutex_unlock(brain->mutex);

    NIMCP_LOGGING_INFO("Hemispheric brain entered split-brain mode");
    return 0;
}

int hemispheric_brain_reconnect_callosum(hemispheric_brain_t* brain) {
    if (!brain || !brain->callosum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_reconnect_callosum: brain or callosum is NULL");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_reconnect_callosum: failed to acquire mutex");
        return -1;
    }

    callosum_reconnect(brain->callosum);
    brain->callosum_intact = true;
    brain->stats.callosum_stats.reconnection_events++;
    nimcp_mutex_unlock(brain->mutex);

    NIMCP_LOGGING_INFO("Hemispheric brain exited split-brain mode");
    return 0;
}

bool hemispheric_brain_is_callosum_intact(const hemispheric_brain_t* brain) {
    return brain ? brain->callosum_intact : false;
}

int hemispheric_brain_set_callosum_bandwidth(
    hemispheric_brain_t* brain,
    callosum_bandwidth_mode_t mode
) {
    if (!brain || !brain->callosum) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_callosum_bandwidth: brain or callosum is NULL");
        return -1;
    }
    return callosum_set_bandwidth_mode(brain->callosum, mode);
}

//=============================================================================
// Resource Management
//=============================================================================

int hemispheric_brain_set_tier(
    hemispheric_brain_t* brain,
    hemisphere_id_t hemisphere,
    platform_tier_t tier
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_tier: brain is NULL");
        return -1;
    }

    brain_hemisphere_t* hemi = (hemisphere == HEMISPHERE_LEFT)
        ? brain->left : brain->right;
    return hemisphere_set_tier(hemi, tier);
}

platform_tier_t hemispheric_brain_get_tier(
    const hemispheric_brain_t* brain,
    hemisphere_id_t hemisphere
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_tier: brain is NULL");
        return PLATFORM_TIER_MINIMAL;
    }

    brain_hemisphere_t* hemi = (hemisphere == HEMISPHERE_LEFT)
        ? brain->left : brain->right;
    return hemisphere_get_tier(hemi);
}

int hemispheric_brain_set_asymmetric_resources(
    hemispheric_brain_t* brain,
    float left_fraction,
    bool rebalance_immediately
) {
    if (!brain || left_fraction < 0.0f || left_fraction > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "hemispheric_brain_set_asymmetric_resources: invalid parameter");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_set_asymmetric_resources: failed to acquire mutex");
        return -1;
    }

    brain->left_resource_fraction = left_fraction;
    brain->asymmetric_resources = (fabsf(left_fraction - 0.5f) > 0.01f);

    if (rebalance_immediately) {
        // Higher fraction = higher tier
        if (left_fraction > 0.6f) {
            hemisphere_set_tier(brain->left, PLATFORM_TIER_FULL);
            hemisphere_set_tier(brain->right, PLATFORM_TIER_CONSTRAINED);
        } else if (left_fraction < 0.4f) {
            hemisphere_set_tier(brain->left, PLATFORM_TIER_CONSTRAINED);
            hemisphere_set_tier(brain->right, PLATFORM_TIER_FULL);
        } else {
            hemisphere_set_tier(brain->left, PLATFORM_TIER_MEDIUM);
            hemisphere_set_tier(brain->right, PLATFORM_TIER_MEDIUM);
        }
    }

    nimcp_mutex_unlock(brain->mutex);
    return 0;
}

int hemispheric_brain_enable_asymmetric_resources(
    hemispheric_brain_t* brain,
    bool enable
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_enable_asymmetric_resources: brain is NULL");
        return -1;
    }
    brain->asymmetric_resources = enable;
    return 0;
}

//=============================================================================
// Processing Mode
//=============================================================================

int hemispheric_brain_set_mode(
    hemispheric_brain_t* brain,
    hemispheric_mode_t mode
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_mode: brain is NULL");
        return -1;
    }
    brain->default_mode = mode;
    return 0;
}

hemispheric_mode_t hemispheric_brain_get_mode(const hemispheric_brain_t* brain) {
    return brain ? brain->default_mode : HEMISPHERIC_MODE_COOPERATIVE;
}

int hemispheric_brain_set_cooperation_strategy(
    hemispheric_brain_t* brain,
    cooperation_strategy_t strategy
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_cooperation_strategy: brain is NULL");
        return -1;
    }
    brain->cooperation_strategy = strategy;
    return 0;
}

//=============================================================================
// State Query
//=============================================================================

int hemispheric_brain_get_stats(
    const hemispheric_brain_t* brain,
    hemispheric_brain_stats_t* stats
) {
    if (!brain || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_get_stats: NULL parameter");
        return -1;
    }

    // Note: Must cast away const to lock mutex for thread-safe read
    // This is safe because we only read stats, not modify brain state
    nimcp_mutex_lock((nimcp_mutex_t*)brain->mutex);

    memcpy(stats, &brain->stats, sizeof(hemispheric_brain_stats_t));
    stats->current_dominant = brain->dominant_hemisphere;

    // Get hemisphere stats
    hemisphere_get_stats(brain->left, &stats->left_stats);
    hemisphere_get_stats(brain->right, &stats->right_stats);

    // Get callosum stats
    if (brain->callosum) {
        callosum_get_stats(brain->callosum, &stats->callosum_stats);
    }

    nimcp_mutex_unlock((nimcp_mutex_t*)brain->mutex);
    return 0;
}

int hemispheric_brain_reset_stats(hemispheric_brain_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_reset_stats: brain is NULL");
        return -1;
    }

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OPERATION_FAILED,
            "hemispheric_brain_reset_stats: failed to acquire mutex");
        return -1;
    }

    memset(&brain->stats, 0, sizeof(hemispheric_brain_stats_t));
    nimcp_mutex_unlock(brain->mutex);

    return 0;
}

float hemispheric_brain_get_energy(const hemispheric_brain_t* brain) {
    if (!brain) return 0.0f;
    return hemisphere_get_energy(brain->left) +
           hemisphere_get_energy(brain->right);
}

bool hemispheric_brain_is_active(const hemispheric_brain_t* brain) {
    return brain ? brain->is_active : false;
}

int hemispheric_brain_set_active(hemispheric_brain_t* brain, bool active) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_set_active: brain is NULL");
        return -1;
    }
    brain->is_active = active;
    hemisphere_set_active(brain->left, active);
    hemisphere_set_active(brain->right, active);
    return 0;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int hemispheric_brain_connect_bio_async(hemispheric_brain_t* brain) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_connect_bio_async: brain is NULL");
        return -1;
    }

    if (brain->bio_async_enabled) return 0;

    bio_module_info_t info = {
        .module_id = BIO_MODULE_HEMISPHERIC_BRAIN,
        .module_name = "hemispheric_brain",
        .inbox_capacity = 64,
        .user_data = brain
    };

    brain->bio_ctx = bio_router_register_module(&info);
    if (brain->bio_ctx) {
        brain->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Hemispheric brain connected to bio-async router");

        // Also connect callosum
        if (brain->callosum) {
            callosum_connect_bio_async(brain->callosum);
        }
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available");
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemispheric_brain_connect_bio_async: validation failed");
    return -1;
}

int hemispheric_brain_disconnect_bio_async(hemispheric_brain_t* brain) {
    if (!brain || !brain->bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_disconnect_bio_async: brain is NULL or not connected");
        return -1;
    }

    if (brain->callosum) {
        callosum_disconnect_bio_async(brain->callosum);
    }

    bio_router_unregister_module(brain->bio_ctx);
    brain->bio_ctx = NULL;
    brain->bio_async_enabled = false;

    return 0;
}

bool hemispheric_brain_is_bio_async_connected(const hemispheric_brain_t* brain) {
    return brain ? brain->bio_async_enabled : false;
}

//=============================================================================
// Immune System Integration
//=============================================================================

int hemispheric_brain_connect_immune(
    hemispheric_brain_t* brain,
    brain_immune_system_t* immune
) {
    if (!brain) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemispheric_brain_connect_immune: brain is NULL");
        return -1;
    }
    brain->immune = immune;
    return 0;
}

brain_immune_system_t* hemispheric_brain_get_immune(
    const hemispheric_brain_t* brain
) {
    return brain ? brain->immune : NULL;
}

//=============================================================================
// Immune Bridge Integration
//=============================================================================

int hemispheric_brain_apply_lateralization_shift(
    hemispheric_brain_t* brain,
    float shift
) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_POINTER, "brain is NULL");

    // Clamp shift to reasonable range
    if (shift < -1.0f) shift = -1.0f;
    if (shift > 1.0f) shift = 1.0f;

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    // Apply shift to all domains in the lateralization profile
    // Positive shift = toward right, negative = toward left
    // Use the lateralization API to shift each domain
    for (int d = 0; d < COGNITIVE_DOMAIN_COUNT; d++) {
        // Shift amount is scaled - positive shift moves toward right (negative shift param)
        lateralization_shift_dominance(&brain->lateralization, (cognitive_domain_t)d, -shift * 0.1f);
    }

    nimcp_mutex_unlock(brain->mutex);

    return NIMCP_SUCCESS;
}

int hemispheric_brain_set_bilateral_mode(
    hemispheric_brain_t* brain,
    bool bilateral
) {
    NIMCP_CHECK_THROW(brain, NIMCP_ERROR_NULL_POINTER, "brain is NULL");

    if (nimcp_mutex_lock(brain->mutex) != 0) {
        NIMCP_LOGGING_ERROR("Failed to acquire hemispheric brain mutex");
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    brain->bilateral_mode = bilateral;

    // In bilateral mode, switch to parallel processing
    if (bilateral) {
        brain->default_mode = HEMISPHERIC_MODE_PARALLEL;
        NIMCP_LOGGING_WARN("Hemispheric brain entering bilateral mode");
    } else {
        brain->default_mode = HEMISPHERIC_MODE_LATERALIZED;
        NIMCP_LOGGING_INFO("Hemispheric brain exiting bilateral mode");
    }

    nimcp_mutex_unlock(brain->mutex);

    return NIMCP_SUCCESS;
}

bool hemispheric_brain_is_bilateral_mode(const hemispheric_brain_t* brain) {
    return brain ? brain->bilateral_mode : false;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* hemispheric_mode_name(hemispheric_mode_t mode) {
    switch (mode) {
        case HEMISPHERIC_MODE_LATERALIZED:
            return "Lateralized";
        case HEMISPHERIC_MODE_PARALLEL:
            return "Parallel";
        case HEMISPHERIC_MODE_COMPETITIVE:
            return "Competitive";
        case HEMISPHERIC_MODE_COOPERATIVE:
            return "Cooperative";
        default:
            return "Unknown";
    }
}

const char* cooperation_strategy_name(cooperation_strategy_t strategy) {
    switch (strategy) {
        case COOPERATION_AVERAGE:
            return "Average";
        case COOPERATION_WEIGHTED:
            return "Weighted";
        case COOPERATION_DOMINANT:
            return "Dominant";
        case COOPERATION_ATTENTION_GATED:
            return "Attention-Gated";
        default:
            return "Unknown";
    }
}
