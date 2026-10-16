//=============================================================================
// nimcp_brain_hemisphere.c - Single Brain Hemisphere Implementation
//=============================================================================
/**
 * @file nimcp_brain_hemisphere.c
 * @brief Implementation of a single brain hemisphere wrapping brain_t
 *
 * BIOLOGICAL BASIS:
 * - Each hemisphere has ~43 billion neurons (half of brain)
 * - Independent neural networks with specialized regions
 * - Contralateral motor/sensory mapping
 * - Local neuromodulator concentrations
 *
 * @author NIMCP Development Team
 * @date 2025-12-22
 */

#include "core/brain/hemispheric/nimcp_brain_hemisphere.h"
#include "core/brain/hemispheric/nimcp_corpus_callosum.h"
#include "core/brain/nimcp_brain_internal.h"  // For access to brain_struct fields
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

BRIDGE_BOILERPLATE_MESH_ONLY(brain_hemisphere, MESH_ADAPTER_CATEGORY_COGNITIVE)


//=============================================================================
// Default Configuration
//=============================================================================

hemisphere_config_t hemisphere_default_config(hemisphere_id_t hemisphere_id) {
    hemisphere_config_t config = {
        .task_name = (hemisphere_id == HEMISPHERE_LEFT)
            ? "left_hemisphere" : "right_hemisphere",
        .size = BRAIN_SIZE_MEDIUM,
        .task = BRAIN_TASK_PATTERN_MATCHING,
        .num_inputs = 64,
        .num_outputs = 32,

        .hemisphere_id = hemisphere_id,
        .specialization_weights = {0.0f},  // Initialized based on lateralization

        .enable_local_neuromod = true,
        .neuromod_diffusion_rate = 0.1f,

        .enable_local_glial = false,  // Disabled for now - requires network param

        .initial_tier = PLATFORM_TIER_MEDIUM,

        .enable_bio_async = true
    };

    // Set default specialization based on biological lateralization
    if (hemisphere_id == HEMISPHERE_LEFT) {
        // Left hemisphere: language, logic, sequential, fine motor (right hand)
        config.specialization_weights[COGNITIVE_DOMAIN_LANGUAGE] = 0.95f;
        config.specialization_weights[COGNITIVE_DOMAIN_LOGICAL_REASONING] = 0.85f;
        config.specialization_weights[COGNITIVE_DOMAIN_MOTOR_FINE] = 0.90f;
        config.specialization_weights[COGNITIVE_DOMAIN_ATTENTION_LOCAL] = 0.75f;
        config.specialization_weights[COGNITIVE_DOMAIN_MUSIC_RHYTHM] = 0.80f;
    } else {
        // Right hemisphere: spatial, emotion, holistic, face recognition
        config.specialization_weights[COGNITIVE_DOMAIN_SPATIAL] = 0.80f;
        config.specialization_weights[COGNITIVE_DOMAIN_EMOTION] = 0.70f;
        config.specialization_weights[COGNITIVE_DOMAIN_FACE_RECOGNITION] = 0.85f;
        config.specialization_weights[COGNITIVE_DOMAIN_ATTENTION_GLOBAL] = 0.75f;
        config.specialization_weights[COGNITIVE_DOMAIN_CREATIVE_THINKING] = 0.65f;
        config.specialization_weights[COGNITIVE_DOMAIN_MUSIC_MELODY] = 0.80f;
    }

    return config;
}

//=============================================================================
// Lifecycle
//=============================================================================

brain_hemisphere_t* hemisphere_create(const hemisphere_config_t* config) {
    if (!config) {
        NIMCP_LOGGING_ERROR("NULL config for hemisphere creation");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_create: config is NULL");
        return NULL;
    }

    brain_hemisphere_t* hemi = nimcp_calloc(1, sizeof(brain_hemisphere_t));
    if (!hemi) {
        NIMCP_LOGGING_ERROR("Failed to allocate hemisphere");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemisphere_create: failed to allocate hemisphere");
        return NULL;
    }

    // Set identity
    hemi->id = config->hemisphere_id;
    hemi->creation_time = nimcp_time_get_us();

    // Create underlying brain using the direct API
    hemi->brain = brain_create(config->task_name, config->size, config->task,
                               config->num_inputs, config->num_outputs);

    // Copy specialization weights
    memcpy(hemi->specialization, config->specialization_weights,
           sizeof(float) * COGNITIVE_DOMAIN_COUNT);

    // Initialize neuromodulator system
    if (config->enable_local_neuromod) {
        hemi->neuromod = neuromodulator_system_create(NULL);  // Use defaults
    }
    hemi->neuromod_diffusion_rate = config->neuromod_diffusion_rate;

    // Glial system - skip for now (requires network parameter)
    hemi->glial = NULL;

    // Initialize contralateral maps
    hemi->motor_map.controls_right_side = (config->hemisphere_id == HEMISPHERE_LEFT);
    hemi->sensory_map.controls_right_side = (config->hemisphere_id == HEMISPHERE_LEFT);

    // State
    hemi->activity_level = 0.0f;
    hemi->energy_consumption = 0.0f;
    hemi->is_active = true;
    hemi->current_tier = config->initial_tier;

    // Bio-async
    hemi->bio_async_enabled = false;

    // Mutex
    hemi->mutex = nimcp_malloc(sizeof(nimcp_mutex_t));
    if (!hemi->mutex) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemisphere_create: failed to allocate mutex");
        brain_destroy(hemi->brain);
        if (hemi->neuromod) {
            neuromodulator_system_destroy(hemi->neuromod);
        }
        nimcp_free(hemi);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemisphere_create: validation failed");
        return NULL;
    }
    nimcp_mutex_init(hemi->mutex, NULL);

    NIMCP_LOGGING_INFO("Created %s hemisphere", hemisphere_name(config->hemisphere_id));

    // Connect bio-async if enabled
    if (config->enable_bio_async) {
        hemisphere_connect_bio_async(hemi);
    }

    return hemi;
}

void hemisphere_destroy(brain_hemisphere_t* hemisphere) {
    if (!hemisphere) {
        return;
    }

    // Disconnect bio-async
    if (hemisphere->bio_async_enabled) {
        hemisphere_disconnect_bio_async(hemisphere);
    }

    // Destroy brain
    brain_destroy(hemisphere->brain);

    // Destroy neuromodulator system
    if (hemisphere->neuromod) {
        neuromodulator_system_destroy(hemisphere->neuromod);
    }

    // Destroy glial system
    if (hemisphere->glial) {
        glial_integration_destroy(hemisphere->glial);
    }

    // Free contralateral maps
    if (hemisphere->motor_map.motor_neuron_ids) {
        nimcp_free(hemisphere->motor_map.motor_neuron_ids);
    }
    if (hemisphere->motor_map.sensory_neuron_ids) {
        nimcp_free(hemisphere->motor_map.sensory_neuron_ids);
    }
    if (hemisphere->sensory_map.motor_neuron_ids) {
        nimcp_free(hemisphere->sensory_map.motor_neuron_ids);
    }
    if (hemisphere->sensory_map.sensory_neuron_ids) {
        nimcp_free(hemisphere->sensory_map.sensory_neuron_ids);
    }

    // Destroy mutex
    if (hemisphere->mutex) {
        nimcp_mutex_free(hemisphere->mutex);
    }

    nimcp_free(hemisphere);
}

//=============================================================================
// Processing
//=============================================================================

int hemisphere_update(brain_hemisphere_t* hemisphere, float dt) {
    if (!hemisphere || !hemisphere->is_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_update: hemisphere is NULL or inactive");
        return -1;
    }

    nimcp_mutex_lock(hemisphere->mutex);

    uint64_t start_time = nimcp_time_get_us();

    // Update neuromodulator system
    if (hemisphere->neuromod) {
        neuromodulator_update(hemisphere->neuromod, dt);
    }

    // Glial updates would go here when connected
    // (glial system requires network parameter, skipped for now)

    // Simulate activity decay/recovery based on neuromodulator levels
    if (hemisphere->neuromod) {
        float dopamine = neuromodulator_get_level(hemisphere->neuromod, NEUROMOD_DOPAMINE);
        float norepinephrine = neuromodulator_get_level(hemisphere->neuromod, NEUROMOD_NOREPINEPHRINE);
        // Activity influenced by arousal neuromodulators
        hemisphere->activity_level = hemisphere->activity_level * 0.95f +
                                     (dopamine * 0.3f + norepinephrine * 0.2f) * 0.05f;
    } else {
        // Default activity decay
        hemisphere->activity_level *= 0.99f;
    }

    // Estimate energy consumption from activity
    hemisphere->energy_consumption = hemisphere->activity_level * 0.1f;

    // Update statistics
    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    hemisphere->stats.total_updates++;
    hemisphere->stats.avg_update_time_ms =
        hemisphere->stats.avg_update_time_ms * 0.99f +
        (elapsed_us / 1000.0f) * 0.01f;

    hemisphere->stats.current_activity = hemisphere->activity_level;
    if (hemisphere->activity_level > hemisphere->stats.peak_activity_level) {
        hemisphere->stats.peak_activity_level = hemisphere->activity_level;
    }
    hemisphere->stats.avg_activity_level =
        hemisphere->stats.avg_activity_level * 0.99f +
        hemisphere->activity_level * 0.01f;

    hemisphere->stats.avg_energy_consumption =
        hemisphere->stats.avg_energy_consumption * 0.99f +
        hemisphere->energy_consumption * 0.01f;

    nimcp_mutex_unlock(hemisphere->mutex);
    return 0;
}

int hemisphere_infer(
    brain_hemisphere_t* hemisphere,
    const float* input,
    uint32_t input_size,
    float* output,
    uint32_t output_size
) {
    if (!hemisphere || !input || !output || !hemisphere->is_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_infer: NULL parameter or hemisphere inactive");
        return -1;
    }

    nimcp_mutex_lock(hemisphere->mutex);

    uint64_t start_time = nimcp_time_get_us();
    int result = 0;

    // Use brain_decide for real neural network inference
    brain_decision_t* decision = brain_decide(hemisphere->brain, input, input_size);

    if (decision) {
        // Copy output vector to caller's buffer
        uint32_t copy_size = (decision->output_size < output_size)
            ? decision->output_size : output_size;

        if (decision->output_vector) {
            memcpy(output, decision->output_vector, copy_size * sizeof(float));
        }

        // Zero-fill any remaining output slots
        for (uint32_t i = copy_size; i < output_size; i++) {
            output[i] = 0.0f;
        }

        // Update activity level from decision confidence
        hemisphere->activity_level = hemisphere->activity_level * 0.9f +
                                     decision->confidence * 0.1f;

        brain_free_decision(decision);
    } else {
        // Fallback: zero output if brain_decide fails
        memset(output, 0, output_size * sizeof(float));
        result = -1;
    }

    uint64_t elapsed_us = nimcp_time_get_us() - start_time;
    // Use atomic increment for thread-safe stats update
    __atomic_fetch_add(&hemisphere->stats.total_inferences, 1, __ATOMIC_RELAXED);
    hemisphere->stats.avg_inference_time_ms =
        hemisphere->stats.avg_inference_time_ms * 0.99f +
        (elapsed_us / 1000.0f) * 0.01f;

    nimcp_mutex_unlock(hemisphere->mutex);
    return result;
}

float hemisphere_train(
    brain_hemisphere_t* hemisphere,
    const float* input,
    const float* target,
    uint32_t size
) {
    if (!hemisphere || !input || !target || !hemisphere->is_active) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_train: NULL parameter or hemisphere inactive");
        return -1.0f;
    }

    // First, run inference to get current output and activate neurons
    float* output = nimcp_malloc(size * sizeof(float));
    if (!output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY,
            "hemisphere_train: failed to allocate output buffer");
        return -1.0f;
    }

    // Run inference (this activates the network and sets eligibility traces)
    int infer_result = hemisphere_infer(hemisphere, input, size, output, size);
    if (infer_result != 0) {
        nimcp_free(output);
        return -1.0f;
    }

    nimcp_mutex_lock(hemisphere->mutex);

    // Compute MSE loss between output and target
    float loss = 0.0f;
    for (uint32_t i = 0; i < size; i++) {
        float diff = output[i] - target[i];
        loss += diff * diff;
    }
    loss /= size;

    // Convert loss to reward signal for reinforcement learning
    // Low loss = high reward (approaching 1.0)
    // High loss = negative reward (approaching -1.0)
    // Using exponential mapping: reward = 2 * exp(-loss) - 1
    float reward = 2.0f * expf(-loss) - 1.0f;

    // Clamp reward to [-1, 1]
    if (reward < -1.0f) reward = -1.0f;
    if (reward > 1.0f) reward = 1.0f;

    // Apply reward-based learning (three-factor rule: Hebbian + Reward + Dopamine)
    // This modifies synapses that were recently active based on the reward signal
    brain_apply_reward_learning(hemisphere->brain, reward);

    // Use atomic increment for thread-safe stats update
    __atomic_fetch_add(&hemisphere->stats.total_learning_steps, 1, __ATOMIC_RELAXED);

    nimcp_mutex_unlock(hemisphere->mutex);
    nimcp_free(output);

    return loss;
}

//=============================================================================
// State Query
//=============================================================================

brain_t hemisphere_get_brain(brain_hemisphere_t* hemisphere) {
    if (!hemisphere) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_get_brain: hemisphere is NULL");
        brain_t empty = {0};
        return empty;
    }
    return hemisphere->brain;
}

float hemisphere_get_activity(const brain_hemisphere_t* hemisphere) {
    return hemisphere ? hemisphere->activity_level : 0.0f;
}

float hemisphere_get_energy(const brain_hemisphere_t* hemisphere) {
    return hemisphere ? hemisphere->energy_consumption : 0.0f;
}

platform_tier_t hemisphere_get_tier(const brain_hemisphere_t* hemisphere) {
    return hemisphere ? hemisphere->current_tier : PLATFORM_TIER_MINIMAL;
}

int hemisphere_get_stats(
    const brain_hemisphere_t* hemisphere,
    hemisphere_stats_t* stats
) {
    if (!hemisphere || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_get_stats: NULL parameter");
        return -1;
    }

    // Note: Must cast away const to lock mutex for thread-safe read
    // This is safe because we only read stats, not modify hemisphere state
    nimcp_mutex_lock((nimcp_mutex_t*)hemisphere->mutex);
    memcpy(stats, &hemisphere->stats, sizeof(hemisphere_stats_t));
    nimcp_mutex_unlock((nimcp_mutex_t*)hemisphere->mutex);

    return 0;
}

float hemisphere_get_specialization(
    const brain_hemisphere_t* hemisphere,
    cognitive_domain_t domain
) {
    if (!hemisphere || domain >= COGNITIVE_DOMAIN_COUNT) {
        return 0.0f;
    }
    return hemisphere->specialization[domain];
}

//=============================================================================
// Resource Management
//=============================================================================

int hemisphere_set_tier(brain_hemisphere_t* hemisphere, platform_tier_t tier) {
    if (!hemisphere) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemisphere is NULL");

        return -1;
    }

    nimcp_mutex_lock(hemisphere->mutex);

    platform_tier_t old_tier = hemisphere->current_tier;
    hemisphere->current_tier = tier;

    if (old_tier != tier) {
        hemisphere->stats.tier_switches++;
        NIMCP_LOGGING_INFO("%s hemisphere tier changed: %d -> %d",
                          hemisphere_name(hemisphere->id), old_tier, tier);
    }

    nimcp_mutex_unlock(hemisphere->mutex);
    return 0;
}

int hemisphere_set_active(brain_hemisphere_t* hemisphere, bool active) {
    if (!hemisphere) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemisphere is NULL");

        return -1;
    }

    nimcp_mutex_lock(hemisphere->mutex);
    hemisphere->is_active = active;
    nimcp_mutex_unlock(hemisphere->mutex);

    return 0;
}

//=============================================================================
// Neuromodulator Control
//=============================================================================

float hemisphere_get_neuromod(
    const brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type
) {
    if (!hemisphere || !hemisphere->neuromod) {
        return 0.0f;
    }
    return neuromodulator_get_level(hemisphere->neuromod, type);
}

int hemisphere_set_neuromod(
    brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type,
    float level
) {
    if (!hemisphere || !hemisphere->neuromod) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_set_neuromod: hemisphere or neuromod is NULL");
        return -1;
    }

    nimcp_mutex_lock(hemisphere->mutex);
    neuromodulator_set_level(hemisphere->neuromod, type, level);
    nimcp_mutex_unlock(hemisphere->mutex);

    return 0;
}

float hemisphere_apply_neuromod_diffusion(
    brain_hemisphere_t* hemisphere,
    neuromodulator_type_t type,
    float other_level
) {
    if (!hemisphere || !hemisphere->neuromod) {
        return 0.0f;
    }

    nimcp_mutex_lock(hemisphere->mutex);

    float current = neuromodulator_get_level(hemisphere->neuromod, type);
    float diff = other_level - current;
    float transfer = diff * hemisphere->neuromod_diffusion_rate;
    float new_level = current + transfer;

    // Clamp to valid range
    if (new_level < 0.0f) new_level = 0.0f;
    if (new_level > 1.0f) new_level = 1.0f;

    neuromodulator_set_level(hemisphere->neuromod, type, new_level);

    nimcp_mutex_unlock(hemisphere->mutex);
    return new_level;
}

//=============================================================================
// Contralateral Mapping
//=============================================================================

int hemisphere_map_motor_output(
    brain_hemisphere_t* hemisphere,
    const float* motor_commands,
    uint32_t num_commands,
    float* body_output
) {
    if (!hemisphere || !motor_commands || !body_output) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_map_motor_output: NULL parameter");
        return -1;
    }

    // For now, direct passthrough (contralateral mapping is implicit)
    // In a full implementation, this would apply topographic mapping
    memcpy(body_output, motor_commands, num_commands * sizeof(float));

    return 0;
}

int hemisphere_map_sensory_input(
    brain_hemisphere_t* hemisphere,
    const float* body_input,
    uint32_t num_inputs,
    float* sensory_input
) {
    if (!hemisphere || !body_input || !sensory_input) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_map_sensory_input: NULL parameter");
        return -1;
    }

    // For now, direct passthrough (contralateral mapping is implicit)
    memcpy(sensory_input, body_input, num_inputs * sizeof(float));

    return 0;
}

//=============================================================================
// Callosum Integration
//=============================================================================

void hemisphere_set_callosum(
    brain_hemisphere_t* hemisphere,
    corpus_callosum_t* callosum
) {
    if (!hemisphere) {
        return;
    }
    hemisphere->callosum = callosum;
}

bool hemisphere_has_callosum(const brain_hemisphere_t* hemisphere) {
    return hemisphere ? (hemisphere->callosum != NULL) : false;
}

//=============================================================================
// Bio-Async Integration
//=============================================================================

int hemisphere_connect_bio_async(brain_hemisphere_t* hemisphere) {
    if (!hemisphere) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "hemisphere is NULL");

        return -1;
    }

    if (hemisphere->bio_async_enabled) {
        return 0;
    }

    bio_module_id_t module_id = (hemisphere->id == HEMISPHERE_LEFT)
        ? BIO_MODULE_LEFT_HEMISPHERE
        : BIO_MODULE_RIGHT_HEMISPHERE;

    bio_module_info_t info = {
        .module_id = module_id,
        .module_name = (hemisphere->id == HEMISPHERE_LEFT)
            ? "left_hemisphere" : "right_hemisphere",
        .inbox_capacity = 32,
        .user_data = hemisphere
    };

    hemisphere->bio_ctx = bio_router_register_module(&info);
    if (hemisphere->bio_ctx) {
        hemisphere->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("%s hemisphere connected to bio-async router",
                          hemisphere_name(hemisphere->id));
        return 0;
    }

    NIMCP_LOGGING_WARN("Bio-async router not available for %s hemisphere",
                       hemisphere_name(hemisphere->id));
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "hemisphere_connect_bio_async: validation failed");
    return -1;
}

int hemisphere_disconnect_bio_async(brain_hemisphere_t* hemisphere) {
    if (!hemisphere || !hemisphere->bio_async_enabled) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "hemisphere_disconnect_bio_async: hemisphere is NULL or not connected");
        return -1;
    }

    bio_router_unregister_module(hemisphere->bio_ctx);
    hemisphere->bio_ctx = NULL;
    hemisphere->bio_async_enabled = false;

    return 0;
}

bool hemisphere_is_bio_async_connected(const brain_hemisphere_t* hemisphere) {
    return hemisphere ? hemisphere->bio_async_enabled : false;
}

//=============================================================================
// Learning Rate Control
//=============================================================================

float brain_hemisphere_get_learning_rate(const brain_hemisphere_t* hemisphere) {
    if (!hemisphere) return 0.001f;  // Default

    // Access the underlying brain's learning rate
    return hemisphere->brain->base_learning_rate;
}

int brain_hemisphere_set_learning_rate(
    brain_hemisphere_t* hemisphere,
    float lr
) {
    if (!hemisphere) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
            "brain_hemisphere_set_learning_rate: hemisphere is NULL");
        return -1;
    }
    if (lr < 0.0f || lr > 1.0f) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM,
            "brain_hemisphere_set_learning_rate: lr out of range [0.0, 1.0]");
        return -2;
    }

    // Set the underlying brain's learning rate
    hemisphere->brain->base_learning_rate = lr;

    return 0;
}
