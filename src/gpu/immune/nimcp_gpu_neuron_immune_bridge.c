/**
 * @file nimcp_gpu_neuron_immune_bridge.c
 * @brief GPU Neuron-Immune System Integration Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-12
 */

#include "gpu/immune/nimcp_gpu_neuron_immune_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/platform/nimcp_platform_time.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(gpu_neuron_immune_bridge)

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/**
 * @brief Get inflammation GPU factor
 *
 * WHAT: Map inflammation level to GPU batch size reduction factor
 * WHY:  Different inflammation levels have different resource impacts
 * HOW:  Return predefined factor based on level
 */
static float get_inflammation_gpu_factor(brain_inflammation_level_t level) {
    float cont = inflammation_level_to_continuous(level);
    return inflammation_compute_factor(cont,
        INFLAMMATION_NONE_GPU_FACTOR,
        INFLAMMATION_STORM_GPU_FACTOR);
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

int gpu_neuron_immune_default_config(gpu_neuron_immune_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;
    }

    /* Enable all features by default */
    config->enable_cytokine_gpu_modulation = true;
    config->enable_gpu_error_immune_response = true;
    config->enable_thermal_regulation = true;
    config->enable_batch_size_modulation = true;
    config->enable_memory_conservation = true;

    /* Default sensitivity (1.0 = normal) */
    config->cytokine_sensitivity = 1.0f;
    config->error_sensitivity = 1.0f;
    config->thermal_sensitivity = 1.0f;

    /* Default batch size limits */
    config->base_batch_size = 256;
    config->min_batch_size = 32;
    config->max_batch_size = 1024;

    /* Default thresholds */
    config->utilization_threshold = GPU_UTILIZATION_HIGH_THRESHOLD;
    config->thermal_threshold_celsius = GPU_THERMAL_WARNING_THRESHOLD;
    config->memory_pressure_threshold = 0.85f;

    return 0;
}

gpu_neuron_immune_bridge_t* gpu_neuron_immune_create(
    const gpu_neuron_immune_config_t* config,
    brain_immune_system_t* immune_system,
    gpu_neural_network_t gpu_network
) {
    /* Guard: require immune system */
    if (!immune_system) {
        NIMCP_LOGGING_ERROR("gpu_neuron_immune_create: immune_system required");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_neuron_immune_create: immune_system is NULL");
        return NULL;
    }

    /* Allocate bridge */
    gpu_neuron_immune_bridge_t* bridge = nimcp_malloc(sizeof(gpu_neuron_immune_bridge_t));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("gpu_neuron_immune_create: allocation failed");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "gpu_neuron_immune_create: bridge is NULL");
        return NULL;
    }

    memset(bridge, 0, sizeof(gpu_neuron_immune_bridge_t));

    /* Apply configuration */
    gpu_neuron_immune_config_t default_config;
    if (!config) {
        gpu_neuron_immune_default_config(&default_config);
        config = &default_config;
    }

    bridge->enable_cytokine_gpu_modulation = config->enable_cytokine_gpu_modulation;
    bridge->enable_gpu_error_immune_response = config->enable_gpu_error_immune_response;
    bridge->enable_thermal_regulation = config->enable_thermal_regulation;
    bridge->enable_batch_size_modulation = config->enable_batch_size_modulation;
    bridge->enable_memory_conservation = config->enable_memory_conservation;

    /* Link systems */
    bridge->immune_system = immune_system;
    bridge->gpu_network = gpu_network;

    /* Configuration */
    bridge->base_batch_size = config->base_batch_size;
    bridge->min_batch_size = config->min_batch_size;
    bridge->max_batch_size = config->max_batch_size;

    /* Initialize timing */
    bridge->last_update_time = nimcp_platform_time_get_ms();
    bridge->error_window_start = bridge->last_update_time;

    /* Create mutex */
    if (bridge_base_init(&bridge->base, 0, "gpu_neuron_immune") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_WARN("gpu_neuron_immune_create: mutex creation failed");
    }

    NIMCP_LOGGING_INFO("gpu_neuron_immune_bridge: created successfully");
    return bridge;
}

void gpu_neuron_immune_destroy(gpu_neuron_immune_bridge_t* bridge) {
    if (!bridge) {
        return;
    }

    /* Disconnect bio-async if connected */
    if (bridge->base.bio_async_enabled) {
        gpu_neuron_immune_disconnect_bio_async(bridge);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
}

/* ============================================================================
 * Immune → GPU API
 * ============================================================================ */

int gpu_neuron_immune_apply_cytokine_effects(gpu_neuron_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_cytokine_gpu_modulation) {
        return NIMCP_SUCCESS;  /* Feature disabled */
    }

    nimcp_platform_mutex_lock(bridge->base.mutex);

    /* Get immune stats */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Reset effects */
    memset(&bridge->cytokine_effects, 0, sizeof(gpu_neuron_cytokine_effects_t));

    /* Compute cytokine-induced reductions from actual cytokine levels */
    bridge->cytokine_effects.il1_batch_reduction =
        stats.cytokine_il1 * CYTOKINE_IL1_GPU_BATCH_IMPACT;
    bridge->cytokine_effects.il6_batch_reduction =
        stats.cytokine_il6 * CYTOKINE_IL6_GPU_BATCH_IMPACT;
    bridge->cytokine_effects.tnf_batch_reduction =
        stats.cytokine_tnf * CYTOKINE_TNF_GPU_BATCH_IMPACT;
    bridge->cytokine_effects.ifn_gamma_batch_reduction =
        stats.cytokine_ifn_gamma * CYTOKINE_IFN_GAMMA_GPU_BATCH_IMPACT;

    /* Total batch size factor */
    float reduction = fabsf(bridge->cytokine_effects.il1_batch_reduction) +
                     fabsf(bridge->cytokine_effects.il6_batch_reduction) +
                     fabsf(bridge->cytokine_effects.tnf_batch_reduction) +
                     fabsf(bridge->cytokine_effects.ifn_gamma_batch_reduction);

    bridge->cytokine_effects.total_batch_factor = nimcp_clamp01(1.0f - reduction);

    /* Kernel throttle, memory, and clock speed factors mirror batch factor */
    bridge->cytokine_effects.kernel_throttle_factor = bridge->cytokine_effects.total_batch_factor;
    bridge->cytokine_effects.memory_allocation_factor = bridge->cytokine_effects.total_batch_factor;
    bridge->cytokine_effects.clock_speed_factor = bridge->cytokine_effects.total_batch_factor;

    bridge->cytokine_modulations++;

    nimcp_platform_mutex_unlock(bridge->base.mutex);
    return NIMCP_SUCCESS;
}

float gpu_neuron_immune_compute_batch_factor(const gpu_neuron_immune_bridge_t* bridge) {
    if (!bridge || !bridge->immune_system) {
        return 1.0f;  /* Normal batch size */
    }

    /* Get inflammation level */
    brain_immune_stats_t stats;
    brain_immune_get_stats(bridge->immune_system, &stats);

    /* Use inflammation level from immune stats */
    brain_inflammation_level_t level = stats.inflammation_level;
    float inflammation_factor = get_inflammation_gpu_factor(level);

    /* Combine with cytokine effects */
    float cytokine_factor = bridge->cytokine_effects.total_batch_factor;

    /* Multiplicative reduction */
    return nimcp_clamp01(inflammation_factor * cytokine_factor);
}

uint32_t gpu_neuron_immune_get_batch_size(const gpu_neuron_immune_bridge_t* bridge) {
    if (!bridge) {
        return 256;  /* Default */
    }

    float factor = gpu_neuron_immune_compute_batch_factor(bridge);
    uint32_t modulated_size = (uint32_t)(bridge->base_batch_size * factor);

    /* Clamp to limits */
    if (modulated_size < bridge->min_batch_size) {
        modulated_size = bridge->min_batch_size;
    }
    if (modulated_size > bridge->max_batch_size) {
        modulated_size = bridge->max_batch_size;
    }

    return modulated_size;
}

/* ============================================================================
 * GPU → Immune API
 * ============================================================================ */

int gpu_neuron_immune_trigger_error_response(
    gpu_neuron_immune_bridge_t* bridge,
    int error_code,
    const char* error_message
) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");

    if (!bridge->enable_gpu_error_immune_response) {
        return NIMCP_SUCCESS;  /* Feature disabled */
    }

    /* Determine severity from error code */
    uint8_t severity = GPU_ERROR_SEVERITY_CUDA_ERROR;
    if (error_code == 0) {
        return NIMCP_SUCCESS;  /* No error */
    }

    /* Present antigen */
    const char* msg = error_message ? error_message : "GPU error";
    uint32_t antigen_id;
    int result = brain_immune_present_antigen(
        bridge->immune_system,
        ANTIGEN_SOURCE_MANUAL,
        (const uint8_t*)msg,
        strlen(msg),
        severity,
        0,  /* node_id */
        &antigen_id
    );

    if (result == 0) {
        bridge->immune_triggers++;
        NIMCP_LOGGING_WARN("gpu_neuron_immune: triggered immune response for GPU error %d", error_code);
    }

    return result;
}

int gpu_neuron_immune_monitor_stress(gpu_neuron_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->immune_system, NIMCP_ERROR_NULL_POINTER, "bridge->immune_system is NULL");
    NIMCP_CHECK_THROW(bridge->gpu_network, NIMCP_ERROR_NULL_POINTER, "bridge->gpu_network is NULL");

    /* Query GPU stats */
    uint64_t total_spikes;
    float avg_firing_rate;
    uint64_t gpu_memory_used;

    if (!gpu_neural_network_get_stats(bridge->gpu_network, &total_spikes, &avg_firing_rate, &gpu_memory_used)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Check utilization stress (simplified - would need actual utilization metric) */
    float utilization = avg_firing_rate / 100.0f;  /* Proxy */
    if (utilization > GPU_UTILIZATION_HIGH_THRESHOLD) {
        bridge->immune_modulation.utilization_stress_level = nimcp_clamp01(
            (utilization - GPU_UTILIZATION_HIGH_THRESHOLD) /
            (1.0f - GPU_UTILIZATION_HIGH_THRESHOLD)
        );
    }

    /* Check memory pressure */
    if (bridge->error_state.memory_total_bytes > 0) {
        float memory_usage = (float)gpu_memory_used / bridge->error_state.memory_total_bytes;
        if (memory_usage > 0.85f) {
            bridge->immune_modulation.memory_stress_level = nimcp_clamp01((memory_usage - 0.85f) / 0.15f);
        }
    }

    /* Trigger immune if stressed */
    float total_stress = bridge->immune_modulation.utilization_stress_level +
                        bridge->immune_modulation.thermal_stress_level +
                        bridge->immune_modulation.memory_stress_level;

    if (total_stress > 1.0f && !bridge->immune_modulation.cytokine_release_triggered) {
        bridge->immune_modulation.should_trigger_immune = true;
        bridge->immune_modulation.antigen_severity = GPU_ERROR_SEVERITY_PERFORMANCE_DROP;

        /* Present antigen for stress */
        uint32_t antigen_id;
        const char* msg = "GPU performance stress";
        brain_immune_present_antigen(
            bridge->immune_system,
            ANTIGEN_SOURCE_MANUAL,
            (const uint8_t*)msg,
            strlen(msg),
            bridge->immune_modulation.antigen_severity,
            0,
            &antigen_id
        );

        bridge->immune_modulation.cytokine_release_triggered = true;
        bridge->immune_triggers++;
    }

    return NIMCP_SUCCESS;
}

int gpu_neuron_immune_update_error_state(gpu_neuron_immune_bridge_t* bridge) {
    /* Guard clauses */
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->gpu_network, NIMCP_ERROR_NULL_POINTER, "bridge->gpu_network is NULL");

    /* Query GPU stats */
    uint64_t total_spikes;
    float avg_firing_rate;
    uint64_t gpu_memory_used;

    if (!gpu_neural_network_get_stats(bridge->gpu_network, &total_spikes, &avg_firing_rate, &gpu_memory_used)) {
        return NIMCP_ERROR_OPERATION_FAILED;
    }

    /* Update error state */
    bridge->error_state.memory_used_bytes = gpu_memory_used;
    bridge->error_state.current_utilization = avg_firing_rate / 100.0f;  /* Proxy */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Bidirectional Update API
 * ============================================================================ */

int gpu_neuron_immune_update(gpu_neuron_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    /* Update error state */
    gpu_neuron_immune_update_error_state(bridge);

    /* Apply cytokine effects */
    gpu_neuron_immune_apply_cytokine_effects(bridge);

    /* Monitor stress */
    gpu_neuron_immune_monitor_stress(bridge);

    bridge->total_updates++;
    bridge->last_update_time = nimcp_platform_time_get_ms();

    return NIMCP_SUCCESS;
}

int gpu_neuron_immune_apply_modulation(gpu_neuron_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(bridge->gpu_network, NIMCP_ERROR_NULL_POINTER, "bridge->gpu_network is NULL");

    if (!bridge->enable_batch_size_modulation) {
        return NIMCP_SUCCESS;  /* Feature disabled */
    }

    /* Get modulated batch size */
    uint32_t batch_size = gpu_neuron_immune_get_batch_size(bridge);

    /* Log if changed significantly */
    if (batch_size < bridge->base_batch_size) {
        bridge->batch_reductions++;
        NIMCP_LOGGING_DEBUG("gpu_neuron_immune: batch size reduced to %u (base %u)",
                           batch_size, bridge->base_batch_size);
    }

    /* Would apply to GPU network configuration here */
    /* gpu_neural_network_set_batch_size(bridge->gpu_network, batch_size); */

    return NIMCP_SUCCESS;
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int gpu_neuron_immune_get_cytokine_effects(
    const gpu_neuron_immune_bridge_t* bridge,
    gpu_neuron_cytokine_effects_t* effects
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(effects, NIMCP_ERROR_NULL_POINTER, "effects is NULL");

    memcpy(effects, &bridge->cytokine_effects, sizeof(gpu_neuron_cytokine_effects_t));
    return NIMCP_SUCCESS;
}

int gpu_neuron_immune_get_error_state(
    const gpu_neuron_immune_bridge_t* bridge,
    gpu_neuron_error_state_t* state
) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
    NIMCP_CHECK_THROW(state, NIMCP_ERROR_NULL_POINTER, "state is NULL");

    memcpy(state, &bridge->error_state, sizeof(gpu_neuron_error_state_t));
    return NIMCP_SUCCESS;
}

bool gpu_neuron_immune_is_throttled(const gpu_neuron_immune_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }

    float factor = gpu_neuron_immune_compute_batch_factor(bridge);
    return factor < 1.0f;
}

float gpu_neuron_immune_get_batch_factor(const gpu_neuron_immune_bridge_t* bridge) {
    return gpu_neuron_immune_compute_batch_factor(bridge);
}

/* ============================================================================
 * Bio-Async Integration API
 * ============================================================================ */

int gpu_neuron_immune_connect_bio_async(gpu_neuron_immune_bridge_t* bridge) {
    NIMCP_CHECK_THROW(bridge, NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

    if (bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;  /* Already connected */
    }

    bio_module_info_t info = {
        .module_id = BIO_MODULE_IMMUNE_GPU_NEURON,
        .module_name = "gpu_neuron_immune_bridge",
        .inbox_capacity = 32,
        .user_data = bridge
    };

    bridge->base.bio_ctx = bio_router_register_module(&info);
    if (bridge->base.bio_ctx) {
        bridge->base.bio_async_enabled = true;
        NIMCP_LOGGING_INFO("gpu_neuron_immune: connected to bio-async router");
        return NIMCP_SUCCESS;
    }

    NIMCP_LOGGING_WARN("gpu_neuron_immune: bio-async router not available");
    return NIMCP_ERROR_OPERATION_FAILED;
}

int gpu_neuron_immune_disconnect_bio_async(gpu_neuron_immune_bridge_t* bridge) {
    if (!bridge || !bridge->base.bio_async_enabled) {
        return NIMCP_SUCCESS;
    }

    if (bridge->base.bio_ctx) {
        bio_router_unregister_module(bridge->base.bio_ctx);
        bridge->base.bio_ctx = NULL;
    }

    bridge->base.bio_async_enabled = false;
    NIMCP_LOGGING_INFO("gpu_neuron_immune: disconnected from bio-async router");

    return NIMCP_SUCCESS;
}

bool gpu_neuron_immune_is_bio_async_connected(const gpu_neuron_immune_bridge_t* bridge) {
    return bridge ? bridge->base.bio_async_enabled : false;
}
