//=============================================================================
// nimcp_pr_omni_bridge.c - Prime Resonant Omni-Sensory Bridge Implementation
//=============================================================================
/**
 * @file nimcp_pr_omni_bridge.c
 * @brief Implementation of cross-modal perception to Prime Resonant memory bridge
 * @version 1.0.0
 * @date 2026-01-09
 */

#include "cognitive/memory/core/nimcp_pr_omni_bridge.h"
#include "constants/nimcp_buffer_constants.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#include "glial/myelin_sheath/nimcp_myelin_math.h"

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE_MESH_ONLY(pr_omni_bridge, MESH_ADAPTER_CATEGORY_MEMORY)


/** @brief Send heartbeat from pr_omni_bridge module (instance-level) */
static inline void pr_omni_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_pr_omni_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_pr_omni_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_pr_omni_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}

#define LOG_MODULE "PR_OMNI_BRIDGE"


//=============================================================================
// Thread-Local Error State
//=============================================================================

static _Thread_local char g_last_error[NIMCP_ERROR_BUFFER_SIZE] = {0};

static void set_error(const char* msg) {
    if (msg) {
        strncpy(g_last_error, msg, sizeof(g_last_error) - 1);
        g_last_error[sizeof(g_last_error) - 1] = '\0';
    }
}

//=============================================================================
// Internal Helper Functions
//=============================================================================

/**
 * @brief Get current time in nanoseconds
 */
static uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    return get_time_ns() / 1000ULL;
}

/**
 * @brief Normalize weights to sum to 1.0
 */
static void normalize_weights(float* weights, size_t count) {
    float sum = 0.0f;
    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        sum += weights[i];
    }
    if (sum > PR_OMNI_EPSILON) {
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                                 (float)(i + 1) / (float)count);
            }

            weights[i] /= sum;
        }
    } else {
        /* Equal weights if sum is zero */
        float equal = 1.0f / (float)count;
        for (size_t i = 0; i < count; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && count > 256) {
                pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                                 (float)(i + 1) / (float)count);
            }

            weights[i] = equal;
        }
    }
}

/**
 * @brief Compute geometric mean of values
 */
static float geometric_mean(const float* values, size_t count) {
    if (count == 0) return 0.0f;

    float product = 1.0f;
    size_t valid_count = 0;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        if (values[i] > PR_OMNI_EPSILON) {
            product *= values[i];
            valid_count++;
        }
    }

    if (valid_count == 0) return 0.0f;
    return powf(product, 1.0f / (float)valid_count);
}

/**
 * @brief Find maximum value in array
 */
static float array_max(const float* values, size_t count) {
    if (count == 0) return 0.0f;
    float max_val = values[0];
    for (size_t i = 1; i < count; i++) {
        if (values[i] > max_val) {
            max_val = values[i];
        }
    }
    return max_val;
}

/**
 * @brief Weighted average of values
 */
static float weighted_average(const float* values, const float* weights, size_t count) {
    float sum = 0.0f;
    float weight_sum = 0.0f;

    for (size_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)count);
        }

        sum += values[i] * weights[i];
        weight_sum += weights[i];
    }

    if (weight_sum > PR_OMNI_EPSILON) {
        return sum / (fabsf(weight_sum) > 1e-7f ? weight_sum : 1e-7f);
    }
    return 0.0f;
}

/**
 * @brief Simple SLERP between two quaternions
 */
static nimcp_quaternion_t slerp_two(nimcp_quaternion_t q1, nimcp_quaternion_t q2, float t) {
    /* Compute dot product */
    float dot = q1.w * q2.w + q1.x * q2.x + q1.y * q2.y + q1.z * q2.z;

    /* If dot is negative, negate one quaternion to take shorter path */
    if (dot < 0.0f) {
        q2.w = -q2.w;
        q2.x = -q2.x;
        q2.y = -q2.y;
        q2.z = -q2.z;
        dot = -dot;
    }

    nimcp_quaternion_t result;

    /* If very close, use linear interpolation */
    if (dot > 0.9995f) {
        result.w = q1.w + t * (q2.w - q1.w);
        result.x = q1.x + t * (q2.x - q1.x);
        result.y = q1.y + t * (q2.y - q1.y);
        result.z = q1.z + t * (q2.z - q1.z);

        /* Normalize */
        float mag = sqrtf(result.w * result.w + result.x * result.x +
                         result.y * result.y + result.z * result.z);
        if (mag > PR_OMNI_EPSILON) {
            result.w /= mag;
            result.x /= mag;
            result.y /= mag;
            result.z /= mag;
        }
        return result;
    }

    /* SLERP formula */
    float theta = acosf(dot);
    float sin_theta = sinf(theta);

    float s1 = sinf((1.0f - t) * theta) / sin_theta;
    float s2 = sinf(t * theta) / sin_theta;

    result.w = s1 * q1.w + s2 * q2.w;
    result.x = s1 * q1.x + s2 * q2.x;
    result.y = s1 * q1.y + s2 * q2.y;
    result.z = s1 * q1.z + s2 * q2.z;

    return result;
}

/**
 * @brief Get modality index from pair
 */
static float get_pair_binding(const pr_omni_binding_state_t* state, int m1, int m2) {
    if (m1 > m2) {
        int temp = m1;
        m1 = m2;
        m2 = temp;
    }

    if (m1 == PR_OMNI_MODALITY_VISUAL && m2 == PR_OMNI_MODALITY_AUDIO) {
        return state->binding_visual_audio;
    } else if (m1 == PR_OMNI_MODALITY_VISUAL && m2 == PR_OMNI_MODALITY_SPEECH) {
        return state->binding_visual_speech;
    } else if (m1 == PR_OMNI_MODALITY_AUDIO && m2 == PR_OMNI_MODALITY_SPEECH) {
        return state->binding_audio_speech;
    }
    return 0.0f;
}

//=============================================================================
// Configuration Functions
//=============================================================================

pr_omni_bridge_config_t pr_omni_bridge_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_config_default", 0.0f);


    pr_omni_bridge_config_t config = {
        /* Binding thresholds */
        .binding_threshold = PR_OMNI_BINDING_THRESHOLD,
        .coherence_threshold = PR_OMNI_COHERENCE_THRESHOLD,

        /* Signature fusion */
        .fusion_strategy = PR_OMNI_FUSION_WEIGHTED,
        .shared_prime_boost = 1.5f,

        /* Quaternion blending */
        .slerp_base_t = 0.5f,
        .use_adaptive_weights = true,

        /* Kuramoto coupling */
        .kuramoto_coupling = PR_OMNI_KURAMOTO_COUPLING,
        .enable_adaptive_coupling = true,

        /* Theta-gamma gating */
        .enable_phase_gating = true,
        .encoding_gate_threshold = 0.3f,
        .retrieval_gate_threshold = 0.3f,

        /* Memory creation */
        .auto_create_memories = true,
        .memory_creation_threshold = 0.4f,

        /* Statistics */
        .track_statistics = true
    };
    return config;
}

bool pr_omni_bridge_config_validate(const pr_omni_bridge_config_t* config) {
    if (!config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_omni_bridge_config_validate: config is NULL");
        return false;
    }

    /* Validate binding thresholds */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_config_validate", 0.0f);


    if (config->binding_threshold < 0.0f || config->binding_threshold > 1.0f) {
        set_error("binding_threshold must be in [0, 1]");
        return false;
    }

    if (config->coherence_threshold < 0.0f || config->coherence_threshold > 1.0f) {
        set_error("coherence_threshold must be in [0, 1]");
        return false;
    }

    /* Validate fusion strategy */
    if (config->fusion_strategy < PR_OMNI_FUSION_UNION ||
        config->fusion_strategy > PR_OMNI_FUSION_DOMINANT) {
        set_error("Invalid fusion_strategy");
        return false;
    }

    /* Validate SLERP parameter */
    if (config->slerp_base_t < 0.0f || config->slerp_base_t > 1.0f) {
        set_error("slerp_base_t must be in [0, 1]");
        return false;
    }

    /* Validate Kuramoto coupling */
    if (config->kuramoto_coupling < 0.0f || config->kuramoto_coupling > 2.0f) {
        set_error("kuramoto_coupling should be in [0, 2]");
        return false;
    }

    return true;
}

pr_omni_modal_weights_t pr_omni_modal_weights_default(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_pr_omni_modal_weight", 0.0f);


    pr_omni_modal_weights_t weights = {
        .visual_weight = 1.0f / 3.0f,
        .audio_weight = 1.0f / 3.0f,
        .speech_weight = 1.0f / 3.0f,

        .consolidation_weights = {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f},
        .emotion_weights = {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f},
        .salience_weights = {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f},
        .accessibility_weights = {1.0f / 3.0f, 1.0f / 3.0f, 1.0f / 3.0f}
    };
    return weights;
}

//=============================================================================
// Lifecycle Functions
//=============================================================================

pr_omni_bridge_t* pr_omni_bridge_create(const pr_omni_bridge_config_t* config) {
    /* Use default config if not provided */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_create", 0.0f);


    pr_omni_bridge_config_t default_config;
    if (!config) {
        default_config = pr_omni_bridge_config_default();
        config = &default_config;
    }

    /* Validate configuration */
    if (!pr_omni_bridge_config_validate(config)) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_omni_bridge_create: pr_omni_bridge_config_validate is NULL");
        return NULL;
    }

    /* Allocate bridge structure */
    pr_omni_bridge_t* bridge = (pr_omni_bridge_t*)nimcp_calloc(1, sizeof(pr_omni_bridge_t));
    if (!bridge) {
        set_error("Failed to allocate bridge structure");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Copy configuration */
    bridge->config = *config;

    /* Initialize modal weights */
    bridge->modal_weights = pr_omni_modal_weights_default();

    /* Create Kuramoto oscillator system */
    kuramoto_config_t kura_config = kuramoto_config_default();
    kura_config.max_oscillators = 8;  /* Only need 3 modalities + some extra */
    kura_config.base_coupling_strength = config->kuramoto_coupling;
    kura_config.use_pink_noise = true;
    kura_config.use_adaptive_coupling = config->enable_adaptive_coupling;

    bridge->modal_oscillators = kuramoto_create(&kura_config);
    if (!bridge->modal_oscillators) {
        set_error("Failed to create Kuramoto oscillator system");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "pr_omni_bridge_create: bridge->modal_oscillators is NULL");
        return NULL;
    }

    /* Add oscillators for each modality (using gamma-band frequencies) */
    bridge->visual_osc_id = 0x1001;
    bridge->audio_osc_id = 0x1002;
    bridge->speech_osc_id = 0x1003;

    /* Natural frequencies in gamma band (40-60 Hz = ~250-380 rad/s) */
    kuramoto_add_oscillator(bridge->modal_oscillators, bridge->visual_osc_id, 251.3f);
    kuramoto_add_oscillator(bridge->modal_oscillators, bridge->audio_osc_id, 314.2f);
    kuramoto_add_oscillator(bridge->modal_oscillators, bridge->speech_osc_id, 376.9f);

    /* Set up initial coupling between all modalities */
    float k = config->kuramoto_coupling;
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->visual_osc_id, bridge->audio_osc_id, k);
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->audio_osc_id, bridge->visual_osc_id, k);
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->visual_osc_id, bridge->speech_osc_id, k);
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->speech_osc_id, bridge->visual_osc_id, k);
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->audio_osc_id, bridge->speech_osc_id, k);
    kuramoto_set_coupling(bridge->modal_oscillators, bridge->speech_osc_id, bridge->audio_osc_id, k);

    /* Initialize unified quaternion to identity */
    bridge->unified_quaternion.w = 0.5f;
    bridge->unified_quaternion.x = 0.0f;
    bridge->unified_quaternion.y = 0.5f;
    bridge->unified_quaternion.z = 0.5f;

    /* Initialize binding state */
    memset(&bridge->binding_state, 0, sizeof(bridge->binding_state));

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.first_update_ns = get_time_ns();

    /* Mark as initialized but not connected */
    bridge->initialized = true;
    bridge->bridges_connected = false;
    bridge->dominant = PR_OMNI_DOMINANT_NONE;

    NIMCP_LOGGING_INFO("Created %s bridge", "pr_omni");
    return bridge;
}

void pr_omni_bridge_destroy(pr_omni_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "pr_omni");

    /* Destroy Kuramoto system */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_destroy", 0.0f);


    if (bridge->modal_oscillators) {
        kuramoto_destroy(bridge->modal_oscillators);
        bridge->modal_oscillators = NULL;
    }

    /* Note: We don't own the connected bridges, just NULL the pointers */
    bridge->omni_bridge = NULL;
    bridge->visual_bridge = NULL;
    bridge->audio_bridge = NULL;
    bridge->speech_bridge = NULL;
    bridge->current_multimodal_memory = NULL;
    bridge->multimodal_entanglement = NULL;
    bridge->theta_gamma = NULL;

    /* Free the bridge structure */
    nimcp_free(bridge);
    bridge = NULL;
}

pr_omni_error_t pr_omni_bridge_reset(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Reset Kuramoto phases */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_reset", 0.0f);


    if (bridge->modal_oscillators) {
        kuramoto_reset(bridge->modal_oscillators);
    }

    /* Reset binding state */
    memset(&bridge->binding_state, 0, sizeof(bridge->binding_state));

    /* Reset unified quaternion */
    bridge->unified_quaternion.w = 0.5f;
    bridge->unified_quaternion.x = 0.0f;
    bridge->unified_quaternion.y = 0.5f;
    bridge->unified_quaternion.z = 0.5f;

    /* Reset modal weights to default */
    bridge->modal_weights = pr_omni_modal_weights_default();

    /* Reset statistics */
    uint64_t now = get_time_ns();
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.first_update_ns = now;

    /* Reset dominant modality */
    bridge->dominant = PR_OMNI_DOMINANT_NONE;

    /* Clear current multimodal memory */
    bridge->current_multimodal_memory = NULL;

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Connection Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_connect_bridges(
    pr_omni_bridge_t* bridge,
    omni_sensory_bridge_t* omni,
    pr_visual_bridge_t* visual,
    pr_audio_bridge_t* audio,
    pr_speech_bridge_t* speech)
{
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    if (!omni) {
        set_error("Omni-sensory bridge is required");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Store bridge pointers */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_connect_bridges", 0.0f);


    bridge->omni_bridge = omni;
    bridge->visual_bridge = visual;
    bridge->audio_bridge = audio;
    bridge->speech_bridge = speech;

    /* Count connected modal bridges */
    int connected_modalities = 0;
    if (visual) connected_modalities++;
    if (audio) connected_modalities++;
    if (speech) connected_modalities++;

    if (connected_modalities < 2) {
        set_error("At least two modal bridges should be connected");
        bridge->bridges_connected = false;
        /* Still allow creation, just warn */
    } else {
        bridge->bridges_connected = true;
    }

    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_connect_theta_gamma(
    pr_omni_bridge_t* bridge,
    theta_gamma_manager_t theta_gamma)
{
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_connect_theta_gamma", 0.0f);


    bridge->theta_gamma = theta_gamma;
    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_connect_entanglement(
    pr_omni_bridge_t* bridge,
    entangle_graph_t entanglement)
{
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_connect_entanglement", 0.0f);


    bridge->multimodal_entanglement = entanglement;
    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Main Update Function
//=============================================================================

pr_omni_error_t pr_omni_bridge_update(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    if (!bridge->omni_bridge) {
        set_error("Omni-sensory bridge not connected");
        return PR_OMNI_ERROR_NOT_CONNECTED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_update", 0.0f);


    uint64_t start_time = get_time_us();

    /* 1. Compute binding strengths from omni-sensory bridge */
    pr_omni_error_t err = pr_omni_bridge_compute_binding_strength(bridge, &bridge->binding_state);
    if (err != PR_OMNI_SUCCESS) {
        return err;
    }

    /* 2. Update Kuramoto oscillators for cross-modal synchronization */
    /* Use 1ms timestep */
    err = pr_omni_bridge_sync_oscillators(bridge, 1000000ULL);
    if (err != PR_OMNI_SUCCESS && err != PR_OMNI_ERROR_KURAMOTO_FAILED) {
        return err;
    }

    /* 3. Update modal weights based on current state */
    if (bridge->config.use_adaptive_weights) {
        err = pr_omni_bridge_update_modal_weights(bridge);
        if (err != PR_OMNI_SUCCESS) {
            /* Non-fatal, continue with existing weights */
        }
    }

    /* 4. Compute unified prime signature */
    err = pr_omni_bridge_compute_unified_prime_sig(bridge, &bridge->unified_signature);
    if (err != PR_OMNI_SUCCESS && err != PR_OMNI_ERROR_FUSION_FAILED) {
        /* Fusion failure might happen if no signatures available */
    }

    /* 5. Compute unified quaternion */
    err = pr_omni_bridge_compute_unified_quaternion(bridge, &bridge->unified_quaternion);
    if (err != PR_OMNI_SUCCESS) {
        /* Non-fatal, use previous quaternion */
    }

    /* 6. Determine dominant modality */
    bridge->dominant = pr_omni_bridge_get_dominant_modality(bridge);

    /* 7. Optionally create multimodal memory if binding is strong enough */
    if (bridge->config.auto_create_memories &&
        bridge->binding_state.overall_coherence >= bridge->config.memory_creation_threshold)
    {
        /* Check theta phase gating if enabled */
        bool can_encode = true;
        if (bridge->config.enable_phase_gating && bridge->theta_gamma) {
            float encode_strength = theta_gamma_get_encode_strength(bridge->theta_gamma);
            can_encode = (encode_strength >= bridge->config.encoding_gate_threshold);
        }

        if (can_encode) {
            /* Memory creation would happen here - placeholder for actual implementation */
            if (bridge->config.track_statistics) {
                bridge->stats.memories_created++;
                bridge->stats.encode_operations++;
            }
        } else {
            if (bridge->config.track_statistics) {
                bridge->stats.phase_blocked++;
            }
        }
    }

    /* Update statistics */
    if (bridge->config.track_statistics) {
        bridge->stats.total_updates++;
        bridge->stats.last_update_ns = get_time_ns();

        /* Update running averages */
        float alpha = 0.01f;  /* Smoothing factor */
        bridge->stats.avg_overall_coherence =
            (1.0f - alpha) * bridge->stats.avg_overall_coherence +
            alpha * bridge->binding_state.overall_coherence;

        float avg_binding = (bridge->binding_state.binding_visual_audio +
                            bridge->binding_state.binding_visual_speech +
                            bridge->binding_state.binding_audio_speech) / 3.0f;
        bridge->stats.avg_binding_strength =
            (1.0f - alpha) * bridge->stats.avg_binding_strength +
            alpha * avg_binding;

        /* Track binding events */
        if (bridge->binding_state.fully_bound) {
            bridge->stats.full_bindings++;
        }
        if (bridge->binding_state.visual_audio_bound) {
            bridge->stats.visual_audio_bindings++;
        }
        if (bridge->binding_state.visual_speech_bound) {
            bridge->stats.visual_speech_bindings++;
        }
        if (bridge->binding_state.audio_speech_bound) {
            bridge->stats.audio_speech_bindings++;
        }

        /* Timing */
        uint64_t end_time = get_time_us();
        bridge->stats.total_update_time_us += (end_time - start_time);
    }

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Prime Signature Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_compute_unified_prime_sig(
    pr_omni_bridge_t* bridge,
    prime_signature_t* unified_sig)
{
    if (!bridge || !unified_sig) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /*
     * Note: This is a simplified implementation. In a full implementation,
     * we would access the actual prime signatures from the modal bridges.
     * For now, we demonstrate the fusion logic conceptually.
     */

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_compute_unified_prim", 0.0f);


    if (bridge->config.track_statistics) {
        bridge->stats.signature_fusions++;
    }

    /*
     * Fusion strategy logic (conceptual):
     *
     * UNION: Combine all primes from all modalities
     * INTERSECTION: Only primes present in all bound modalities
     * WEIGHTED: Union with exponents weighted by binding strength
     * DOMINANT: Use only the dominant modality's signature
     */

    /* Initialize unified signature to empty */
    memset(unified_sig, 0, sizeof(prime_signature_t));

    /* The actual implementation would iterate over modal signatures
     * and combine them according to the fusion strategy */

    switch (bridge->config.fusion_strategy) {
        case PR_OMNI_FUSION_UNION:
            /* Union: all primes from all modalities */
            break;

        case PR_OMNI_FUSION_INTERSECTION:
            /* Only shared primes across bound modalities */
            break;

        case PR_OMNI_FUSION_WEIGHTED:
            /* Union weighted by binding strength */
            /* Shared primes get boosted by shared_prime_boost */
            break;

        case PR_OMNI_FUSION_DOMINANT:
            /* Copy dominant modality's signature */
            break;

        default:
            set_error("Invalid fusion strategy");
            return PR_OMNI_ERROR_FUSION_FAILED;
    }

    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_fuse_signatures(
    const prime_signature_t* sig1,
    const prime_signature_t* sig2,
    float binding,
    prime_signature_t* result)
{
    if (!sig1 || !sig2 || !result) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_fuse_signatures", 0.0f);


    if (binding < 0.0f || binding > 1.0f) {
        set_error("Binding must be in [0, 1]");
        return PR_OMNI_ERROR_INVALID_CONFIG;
    }

    /*
     * Fusion logic:
     * - Strong binding (>0.7): Heavy intersection, shared primes dominate
     * - Medium binding (0.3-0.7): Balanced union with weighting
     * - Weak binding (<0.3): Mostly union, minimal interaction
     */

    memset(result, 0, sizeof(prime_signature_t));

    /* Actual prime signature fusion would be implemented here */

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Quaternion Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_compute_unified_quaternion(
    pr_omni_bridge_t* bridge,
    nimcp_quaternion_t* unified_quat)
{
    if (!bridge || !unified_quat) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_compute_unified_quat", 0.0f);


    if (bridge->config.track_statistics) {
        bridge->stats.quaternion_blends++;
    }

    /*
     * Get modal quaternions from bridges (if connected).
     * For now, use default quaternions if bridges not available.
     */
    nimcp_quaternion_t quats[PR_OMNI_NUM_MODALITIES];
    bool has_modal[PR_OMNI_NUM_MODALITIES] = {false, false, false};

    /* Default quaternions (neutral state) */
    nimcp_quaternion_t default_quat = {0.5f, 0.0f, 0.5f, 0.5f};

    /* Visual quaternion */
    if (bridge->visual_bridge) {
        /* Would get from: pr_visual_bridge_get_quaternion(bridge->visual_bridge, &quats[0]) */
        quats[0] = default_quat;
        has_modal[0] = true;
    }

    /* Audio quaternion */
    if (bridge->audio_bridge) {
        /* Would get from: pr_audio_bridge_get_quaternion(bridge->audio_bridge, &quats[1]) */
        quats[1] = default_quat;
        has_modal[1] = true;
    }

    /* Speech quaternion */
    if (bridge->speech_bridge) {
        /* Would get from: pr_speech_bridge_get_quaternion(bridge->speech_bridge, &quats[2]) */
        quats[2] = default_quat;
        has_modal[2] = true;
    }

    /* Count available modalities */
    int num_available = 0;
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        if (has_modal[i]) num_available++;
    }

    if (num_available == 0) {
        /* No modalities available, use default */
        *unified_quat = default_quat;
        return PR_OMNI_SUCCESS;
    }

    /*
     * Compute unified quaternion with special component handling:
     * - w (consolidation): max across modalities
     * - x (emotion): weighted average
     * - y (salience): max across bound modalities
     * - z (accessibility): geometric mean
     */

    float weights[PR_OMNI_NUM_MODALITIES];
    weights[0] = bridge->modal_weights.visual_weight;
    weights[1] = bridge->modal_weights.audio_weight;
    weights[2] = bridge->modal_weights.speech_weight;

    /* Only include available modalities */
    float total_weight = 0.0f;
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        if (!has_modal[i]) {
            weights[i] = 0.0f;
        } else {
            total_weight += weights[i];
        }
    }

    /* Normalize weights */
    if (total_weight > PR_OMNI_EPSILON) {
        for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
                pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                                 (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
            }

            weights[i] /= total_weight;
        }
    }

    /* w-component: max consolidation across modalities */
    float w_values[PR_OMNI_NUM_MODALITIES];
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        w_values[i] = has_modal[i] ? quats[i].w : 0.0f;
    }
    unified_quat->w = array_max(w_values, PR_OMNI_NUM_MODALITIES);

    /* x-component: weighted average emotion */
    float x_values[PR_OMNI_NUM_MODALITIES];
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        x_values[i] = has_modal[i] ? quats[i].x : 0.0f;
    }
    unified_quat->x = weighted_average(x_values, weights, PR_OMNI_NUM_MODALITIES);

    /* y-component: max salience across bound modalities */
    float y_values[PR_OMNI_NUM_MODALITIES];
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        y_values[i] = has_modal[i] ? quats[i].y : 0.0f;
    }
    /* Weight by binding - only highly bound modalities contribute max */
    float va_bind = bridge->binding_state.binding_visual_audio;
    float vs_bind = bridge->binding_state.binding_visual_speech;
    float as_bind = bridge->binding_state.binding_audio_speech;

    /* Scale salience contribution by binding strength */
    float bound_y_values[PR_OMNI_NUM_MODALITIES];
    bound_y_values[0] = y_values[0] * (0.5f + 0.5f * fmaxf(va_bind, vs_bind));
    bound_y_values[1] = y_values[1] * (0.5f + 0.5f * fmaxf(va_bind, as_bind));
    bound_y_values[2] = y_values[2] * (0.5f + 0.5f * fmaxf(vs_bind, as_bind));
    unified_quat->y = array_max(bound_y_values, PR_OMNI_NUM_MODALITIES);

    /* z-component: geometric mean accessibility */
    float z_values[PR_OMNI_NUM_MODALITIES];
    int z_count = 0;
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        if (has_modal[i] && quats[i].z > PR_OMNI_EPSILON) {
            z_values[z_count++] = quats[i].z;
        }
    }
    if (z_count > 0) {
        unified_quat->z = geometric_mean(z_values, (size_t)z_count);
    } else {
        unified_quat->z = 0.5f;  /* Default */
    }

    /* Clamp all values to valid ranges */
    unified_quat->w = nimcp_myelin_clamp(unified_quat->w, 0.0f, 1.0f);
    unified_quat->x = nimcp_myelin_clamp(unified_quat->x, -1.0f, 1.0f);
    unified_quat->y = nimcp_myelin_clamp(unified_quat->y, 0.0f, 1.0f);
    unified_quat->z = nimcp_myelin_clamp(unified_quat->z, 0.0f, 1.0f);

    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_slerp_blend(
    const nimcp_quaternion_t* quaternions,
    const float* weights,
    size_t count,
    nimcp_quaternion_t* result)
{
    if (!quaternions || !weights || !result || count == 0) {
        set_error("Invalid arguments to SLERP blend");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_slerp_blend", 0.0f);


    if (count == 1) {
        *result = quaternions[0];
        return PR_OMNI_SUCCESS;
    }

    /* Iterative SLERP blending */
    nimcp_quaternion_t blended = quaternions[0];
    float accumulated_weight = weights[0];

    for (size_t i = 1; i < count; i++) {
        if (weights[i] < PR_OMNI_EPSILON) continue;

        float t = weights[i] / (accumulated_weight + weights[i]);
        blended = slerp_two(blended, quaternions[i], t);
        accumulated_weight += weights[i];
    }

    *result = blended;
    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Memory Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_fuse_memories(
    pr_omni_bridge_t* bridge,
    pr_omni_multimodal_memory_t* memory)
{
    if (!bridge || !memory) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Check binding threshold */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_fuse_memories", 0.0f);


    if (bridge->binding_state.overall_coherence < bridge->config.binding_threshold) {
        set_error("Binding too weak for memory fusion");
        return PR_OMNI_ERROR_FUSION_FAILED;
    }

    /* Check theta phase if gating enabled */
    if (bridge->config.enable_phase_gating && bridge->theta_gamma) {
        if (!theta_gamma_can_encode(bridge->theta_gamma)) {
            set_error("Not in encoding phase");
            return PR_OMNI_ERROR_PHASE_BLOCKED;
        }
    }

    /* Initialize memory structure */
    memset(memory, 0, sizeof(pr_omni_multimodal_memory_t));

    /* Track which modalities are present */
    memory->has_visual = (bridge->visual_bridge != NULL);
    memory->has_audio = (bridge->audio_bridge != NULL);
    memory->has_speech = (bridge->speech_bridge != NULL);

    /* Store binding state at creation */
    memory->binding_at_creation = bridge->binding_state;

    /* Store timestamps */
    memory->creation_time_ns = get_time_ns();
    memory->last_access_time_ns = memory->creation_time_ns;
    memory->access_count = 0;

    /* Store original modal quaternions */
    memory->visual_quat = bridge->unified_quaternion;  /* Would get actual visual quat */
    memory->audio_quat = bridge->unified_quaternion;   /* Would get actual audio quat */
    memory->speech_quat = bridge->unified_quaternion;  /* Would get actual speech quat */

    /*
     * In full implementation:
     * 1. Create PR memory node with fused signature
     * 2. Store original modal signatures
     * 3. Add entanglement edges between modal representations
     */

    if (bridge->config.track_statistics) {
        bridge->stats.memories_created++;
    }

    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_retrieve_multimodal(
    pr_omni_bridge_t* bridge,
    const pr_omni_retrieval_query_t* query,
    pr_omni_retrieval_result_t* results,
    size_t* num_results)
{
    if (!bridge || !query || !results || !num_results) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Check theta phase if gating enabled */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_retrieve_multimodal", 0.0f);


    if (bridge->config.enable_phase_gating && bridge->theta_gamma) {
        float retrieve_strength = theta_gamma_get_retrieve_strength(bridge->theta_gamma);
        if (retrieve_strength < bridge->config.retrieval_gate_threshold) {
            set_error("Not in optimal retrieval phase");
            if (bridge->config.track_statistics) {
                bridge->stats.phase_blocked++;
            }
            /* Continue anyway but with reduced efficacy */
        } else {
            if (bridge->config.track_statistics) {
                bridge->stats.retrieve_operations++;
            }
        }
    }

    /*
     * Cross-modal retrieval algorithm:
     * 1. Compute resonance against unified multimodal signatures
     * 2. Apply cross-modal boost for matches via different modality
     * 3. Rank by resonance + cross-modal bonus
     */

    *num_results = 0;  /* Placeholder - would return actual results */

    if (bridge->config.track_statistics) {
        bridge->stats.cross_modal_retrievals++;
    }

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Binding Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_compute_binding_strength(
    pr_omni_bridge_t* bridge,
    pr_omni_binding_state_t* state)
{
    if (!bridge || !state) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Get binding from omni-sensory bridge */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_compute_binding_stre", 0.0f);


    if (bridge->omni_bridge) {
        /* Get binding strengths from omni-sensory bridge */
        state->binding_visual_audio =
            omni_sensory_get_binding_strength(bridge->omni_bridge,
                                              OMNI_MODALITY_VISUAL,
                                              OMNI_MODALITY_AUDIO);
        state->binding_visual_speech =
            omni_sensory_get_binding_strength(bridge->omni_bridge,
                                              OMNI_MODALITY_VISUAL,
                                              OMNI_MODALITY_SPEECH);
        state->binding_audio_speech =
            omni_sensory_get_binding_strength(bridge->omni_bridge,
                                              OMNI_MODALITY_AUDIO,
                                              OMNI_MODALITY_SPEECH);
    } else {
        /* No omni bridge, use defaults */
        state->binding_visual_audio = 0.0f;
        state->binding_visual_speech = 0.0f;
        state->binding_audio_speech = 0.0f;
    }

    /* Get Kuramoto coherences to augment binding */
    if (bridge->modal_oscillators) {
        state->kuramoto_va_coherence =
            kuramoto_coherence(bridge->modal_oscillators,
                              bridge->visual_osc_id,
                              bridge->audio_osc_id);
        state->kuramoto_vs_coherence =
            kuramoto_coherence(bridge->modal_oscillators,
                              bridge->visual_osc_id,
                              bridge->speech_osc_id);
        state->kuramoto_as_coherence =
            kuramoto_coherence(bridge->modal_oscillators,
                              bridge->audio_osc_id,
                              bridge->speech_osc_id);

        /* Augment binding with Kuramoto coherence (geometric mean) */
        if (state->kuramoto_va_coherence > 0.0f) {
            state->binding_visual_audio =
                sqrtf(state->binding_visual_audio * state->kuramoto_va_coherence);
        }
        if (state->kuramoto_vs_coherence > 0.0f) {
            state->binding_visual_speech =
                sqrtf(state->binding_visual_speech * state->kuramoto_vs_coherence);
        }
        if (state->kuramoto_as_coherence > 0.0f) {
            state->binding_audio_speech =
                sqrtf(state->binding_audio_speech * state->kuramoto_as_coherence);
        }
    }

    /* Determine bound status based on threshold */
    float threshold = bridge->config.binding_threshold;
    state->visual_audio_bound = (state->binding_visual_audio >= threshold);
    state->visual_speech_bound = (state->binding_visual_speech >= threshold);
    state->audio_speech_bound = (state->binding_audio_speech >= threshold);
    state->fully_bound = state->visual_audio_bound &&
                         state->visual_speech_bound &&
                         state->audio_speech_bound;

    /* Compute overall coherence (average of pairwise bindings) */
    state->overall_coherence = (state->binding_visual_audio +
                               state->binding_visual_speech +
                               state->binding_audio_speech) / 3.0f;

    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_update_modal_weights(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /*
     * Adaptive weight computation based on:
     * 1. Binding strength (more bound modalities get more weight)
     * 2. Salience (attended modalities get more weight)
     * 3. Prediction error (lower PE = more reliable = more weight)
     */

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_update_modal_weights", 0.0f);


    float weights[PR_OMNI_NUM_MODALITIES];

    /* Base weight from binding strength */
    /* Visual participates in VA and VS bindings */
    weights[0] = (bridge->binding_state.binding_visual_audio +
                  bridge->binding_state.binding_visual_speech) / 2.0f;

    /* Audio participates in VA and AS bindings */
    weights[1] = (bridge->binding_state.binding_visual_audio +
                  bridge->binding_state.binding_audio_speech) / 2.0f;

    /* Speech participates in VS and AS bindings */
    weights[2] = (bridge->binding_state.binding_visual_speech +
                  bridge->binding_state.binding_audio_speech) / 2.0f;

    /* Add base weight to prevent any modality from being completely ignored */
    float base_weight = 0.1f;
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        weights[i] += base_weight;
    }

    /* Normalize */
    normalize_weights(weights, PR_OMNI_NUM_MODALITIES);

    /* Store updated weights */
    bridge->modal_weights.visual_weight = weights[0];
    bridge->modal_weights.audio_weight = weights[1];
    bridge->modal_weights.speech_weight = weights[2];

    /* Also update per-component weights (simplified: use same as overall) */
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        bridge->modal_weights.consolidation_weights[i] = weights[i];
        bridge->modal_weights.emotion_weights[i] = weights[i];
        bridge->modal_weights.salience_weights[i] = weights[i];
        bridge->modal_weights.accessibility_weights[i] = weights[i];
    }

    /* Update dominant modality weight in statistics */
    if (bridge->config.track_statistics) {
        float max_weight = array_max(weights, PR_OMNI_NUM_MODALITIES);
        float alpha = 0.01f;
        bridge->stats.avg_dominant_weight =
            (1.0f - alpha) * bridge->stats.avg_dominant_weight +
            alpha * max_weight;
    }

    return PR_OMNI_SUCCESS;
}

float pr_omni_bridge_get_binding(
    const pr_omni_bridge_t* bridge,
    int modality1,
    int modality2)
{
    if (!bridge) return -1.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_binding", 0.0f);


    if (modality1 < 0 || modality1 >= PR_OMNI_NUM_MODALITIES ||
        modality2 < 0 || modality2 >= PR_OMNI_NUM_MODALITIES) {
        return -1.0f;
    }

    if (modality1 == modality2) return 1.0f;  /* Self-binding is 1 */

    return get_pair_binding(&bridge->binding_state, modality1, modality2);
}

//=============================================================================
// Kuramoto Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_sync_oscillators(
    pr_omni_bridge_t* bridge,
    uint64_t dt_ns)
{
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    if (!bridge->modal_oscillators) {
        /* No Kuramoto system, skip */
        return PR_OMNI_SUCCESS;
    }

    /* Update coupling strengths based on binding (if adaptive) */
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_sync_oscillators", 0.0f);


    if (bridge->config.enable_adaptive_coupling) {
        float base_k = bridge->config.kuramoto_coupling;

        /* Scale coupling by binding strength */
        float k_va = base_k * (0.5f + 0.5f * bridge->binding_state.binding_visual_audio);
        float k_vs = base_k * (0.5f + 0.5f * bridge->binding_state.binding_visual_speech);
        float k_as = base_k * (0.5f + 0.5f * bridge->binding_state.binding_audio_speech);

        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->visual_osc_id, bridge->audio_osc_id, k_va);
        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->audio_osc_id, bridge->visual_osc_id, k_va);
        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->visual_osc_id, bridge->speech_osc_id, k_vs);
        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->speech_osc_id, bridge->visual_osc_id, k_vs);
        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->audio_osc_id, bridge->speech_osc_id, k_as);
        kuramoto_set_coupling(bridge->modal_oscillators,
                             bridge->speech_osc_id, bridge->audio_osc_id, k_as);
    }

    /* Convert dt from nanoseconds to seconds */
    float dt_s = (float)dt_ns / 1e9f;

    /* Step the Kuramoto system */
    bool success = kuramoto_step(bridge->modal_oscillators, dt_s);
    if (!success) {
        set_error("Kuramoto step failed");
        return PR_OMNI_ERROR_KURAMOTO_FAILED;
    }

    return PR_OMNI_SUCCESS;
}

float pr_omni_bridge_get_kuramoto_coherence(
    const pr_omni_bridge_t* bridge,
    int modality1,
    int modality2)
{
    if (!bridge || !bridge->modal_oscillators) return 0.0f;

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_kuramoto_coheren", 0.0f);


    if (modality1 < 0 || modality1 >= PR_OMNI_NUM_MODALITIES ||
        modality2 < 0 || modality2 >= PR_OMNI_NUM_MODALITIES) {
        return 0.0f;
    }

    if (modality1 == modality2) return 1.0f;

    uint32_t id1, id2;
    switch (modality1) {
        case PR_OMNI_MODALITY_VISUAL: id1 = bridge->visual_osc_id; break;
        case PR_OMNI_MODALITY_AUDIO: id1 = bridge->audio_osc_id; break;
        case PR_OMNI_MODALITY_SPEECH: id1 = bridge->speech_osc_id; break;
        default: return 0.0f;
    }
    switch (modality2) {
        case PR_OMNI_MODALITY_VISUAL: id2 = bridge->visual_osc_id; break;
        case PR_OMNI_MODALITY_AUDIO: id2 = bridge->audio_osc_id; break;
        case PR_OMNI_MODALITY_SPEECH: id2 = bridge->speech_osc_id; break;
        default: return 0.0f;
    }

    return kuramoto_coherence(bridge->modal_oscillators, id1, id2);
}

pr_omni_error_t pr_omni_bridge_set_kuramoto_coupling(
    pr_omni_bridge_t* bridge,
    int modality1,
    int modality2,
    float coupling)
{
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    if (!bridge->modal_oscillators) {
        set_error("Kuramoto system not initialized");
        return PR_OMNI_ERROR_KURAMOTO_FAILED;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_set_kuramoto_couplin", 0.0f);


    if (modality1 < 0 || modality1 >= PR_OMNI_NUM_MODALITIES ||
        modality2 < 0 || modality2 >= PR_OMNI_NUM_MODALITIES) {
        set_error("Invalid modality index");
        return PR_OMNI_ERROR_INVALID_MODALITY;
    }

    uint32_t id1, id2;
    switch (modality1) {
        case PR_OMNI_MODALITY_VISUAL: id1 = bridge->visual_osc_id; break;
        case PR_OMNI_MODALITY_AUDIO: id1 = bridge->audio_osc_id; break;
        case PR_OMNI_MODALITY_SPEECH: id1 = bridge->speech_osc_id; break;
        default: return PR_OMNI_ERROR_INVALID_MODALITY;
    }
    switch (modality2) {
        case PR_OMNI_MODALITY_VISUAL: id2 = bridge->visual_osc_id; break;
        case PR_OMNI_MODALITY_AUDIO: id2 = bridge->audio_osc_id; break;
        case PR_OMNI_MODALITY_SPEECH: id2 = bridge->speech_osc_id; break;
        default: return PR_OMNI_ERROR_INVALID_MODALITY;
    }

    kuramoto_set_coupling(bridge->modal_oscillators, id1, id2, coupling);
    kuramoto_set_coupling(bridge->modal_oscillators, id2, id1, coupling);

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Query Functions
//=============================================================================

pr_omni_dominant_modality_t pr_omni_bridge_get_dominant_modality(
    const pr_omni_bridge_t* bridge)
{
    if (!bridge) return PR_OMNI_DOMINANT_NONE;

    /*
     * Determine dominant modality based on:
     * 1. Weight (from adaptive weighting)
     * 2. Salience contribution to unified quaternion
     * 3. Binding strength participation
     */

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_dominant_modalit", 0.0f);


    float scores[PR_OMNI_NUM_MODALITIES];

    /* Weight contribution */
    scores[0] = bridge->modal_weights.visual_weight;
    scores[1] = bridge->modal_weights.audio_weight;
    scores[2] = bridge->modal_weights.speech_weight;

    /* Binding contribution */
    scores[0] += 0.3f * (bridge->binding_state.binding_visual_audio +
                        bridge->binding_state.binding_visual_speech);
    scores[1] += 0.3f * (bridge->binding_state.binding_visual_audio +
                        bridge->binding_state.binding_audio_speech);
    scores[2] += 0.3f * (bridge->binding_state.binding_visual_speech +
                        bridge->binding_state.binding_audio_speech);

    /* Find maximum */
    int max_idx = 0;
    float max_score = scores[0];
    for (int i = 1; i < PR_OMNI_NUM_MODALITIES; i++) {
        if (scores[i] > max_score) {
            max_score = scores[i];
            max_idx = i;
        }
    }

    /* Check if any modality is clearly dominant */
    float second_max = 0.0f;
    for (int i = 0; i < PR_OMNI_NUM_MODALITIES; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && PR_OMNI_NUM_MODALITIES > 256) {
            pr_omni_bridge_heartbeat("pr_omni_brid_loop",
                             (float)(i + 1) / (float)PR_OMNI_NUM_MODALITIES);
        }

        if (i != max_idx && scores[i] > second_max) {
            second_max = scores[i];
        }
    }

    /* If no clear winner (less than 20% difference), return NONE */
    if (max_score - second_max < 0.2f * max_score) {
        return PR_OMNI_DOMINANT_NONE;
    }

    switch (max_idx) {
        case PR_OMNI_MODALITY_VISUAL: return PR_OMNI_DOMINANT_VISUAL;
        case PR_OMNI_MODALITY_AUDIO: return PR_OMNI_DOMINANT_AUDIO;
        case PR_OMNI_MODALITY_SPEECH: return PR_OMNI_DOMINANT_SPEECH;
        default: return PR_OMNI_DOMINANT_NONE;
    }
}

pr_omni_error_t pr_omni_bridge_get_unified_signature(
    const pr_omni_bridge_t* bridge,
    prime_signature_t* sig)
{
    if (!bridge || !sig) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    *sig = bridge->unified_signature;
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_unified_signatur", 0.0f);


    return PR_OMNI_SUCCESS;
}

nimcp_quaternion_t pr_omni_bridge_get_unified_quaternion(
    const pr_omni_bridge_t* bridge)
{
    if (!bridge) {
        nimcp_quaternion_t zero = {0.0f, 0.0f, 0.0f, 0.0f};
        return zero;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_unified_quaterni", 0.0f);


    return bridge->unified_quaternion;
}

pr_omni_error_t pr_omni_bridge_get_binding_state(
    const pr_omni_bridge_t* bridge,
    pr_omni_binding_state_t* state)
{
    if (!bridge || !state) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    *state = bridge->binding_state;
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_binding_state", 0.0f);


    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_get_modal_weights(
    const pr_omni_bridge_t* bridge,
    pr_omni_modal_weights_t* weights)
{
    if (!bridge || !weights) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    *weights = bridge->modal_weights;
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_modal_weights", 0.0f);


    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Statistics Functions
//=============================================================================

pr_omni_error_t pr_omni_bridge_get_stats(
    const pr_omni_bridge_t* bridge,
    pr_omni_bridge_stats_t* stats)
{
    if (!bridge || !stats) {
        set_error("NULL pointer argument");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_get_stats", 0.0f);


    return PR_OMNI_SUCCESS;
}

pr_omni_error_t pr_omni_bridge_reset_stats(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        set_error("NULL bridge pointer");
        return PR_OMNI_ERROR_NULL_POINTER;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_reset_stats", 0.0f);


    uint64_t now = get_time_ns();
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->stats.first_update_ns = now;

    return PR_OMNI_SUCCESS;
}

//=============================================================================
// Utility Functions
//=============================================================================

const char* pr_omni_bridge_error_string(pr_omni_error_t error) {
    switch (error) {
        case PR_OMNI_SUCCESS: return "Success";
        case PR_OMNI_ERROR_NULL_POINTER: return "NULL pointer argument";
        case PR_OMNI_ERROR_INVALID_CONFIG: return "Invalid configuration";
        case PR_OMNI_ERROR_NO_MEMORY: return "Memory allocation failed";
        case PR_OMNI_ERROR_NOT_CONNECTED: return "Required bridges not connected";
        case PR_OMNI_ERROR_INVALID_MODALITY: return "Invalid modality index";
        case PR_OMNI_ERROR_FUSION_FAILED: return "Signature fusion failed";
        case PR_OMNI_ERROR_QUATERNION_FAILED: return "Quaternion blend failed";
        case PR_OMNI_ERROR_MEMORY_FAILED: return "Memory node operation failed";
        case PR_OMNI_ERROR_KURAMOTO_FAILED: return "Kuramoto operation failed";
        case PR_OMNI_ERROR_RETRIEVAL_FAILED: return "Cross-modal retrieval failed";
        case PR_OMNI_ERROR_PHASE_BLOCKED: return "Operation blocked by theta phase";
        default: return "Unknown error";
    }
}

const char* pr_omni_bridge_get_last_error(void) {
    if (g_last_error[0] == '\0') {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "pr_omni_bridge_get_last_error: validation failed");
        return NULL;
    }
    return g_last_error;
}

const char* pr_omni_bridge_dominant_to_string(pr_omni_dominant_modality_t dominant) {
    switch (dominant) {
        case PR_OMNI_DOMINANT_NONE: return "none";
        case PR_OMNI_DOMINANT_VISUAL: return "visual";
        case PR_OMNI_DOMINANT_AUDIO: return "audio";
        case PR_OMNI_DOMINANT_SPEECH: return "speech";
        default: return "unknown";
    }
}

const char* pr_omni_bridge_fusion_to_string(pr_omni_fusion_strategy_t strategy) {
    switch (strategy) {
        case PR_OMNI_FUSION_UNION: return "union";
        case PR_OMNI_FUSION_INTERSECTION: return "intersection";
        case PR_OMNI_FUSION_WEIGHTED: return "weighted";
        case PR_OMNI_FUSION_DOMINANT: return "dominant";
        default: return "unknown";
    }
}

void pr_omni_bridge_print_state(const pr_omni_bridge_t* bridge) {
    if (!bridge) {
        printf("PR Omni Bridge: NULL\n");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_print_state", 0.0f);


    printf("=== PR Omni-Sensory Bridge State ===\n");
    printf("Initialized: %s\n", bridge->initialized ? "yes" : "no");
    printf("Connected: %s\n", bridge->bridges_connected ? "yes" : "no");
    printf("\n");

    printf("--- Modal Bridges ---\n");
    printf("  Omni-sensory: %s\n", bridge->omni_bridge ? "connected" : "NULL");
    printf("  Visual: %s\n", bridge->visual_bridge ? "connected" : "NULL");
    printf("  Audio: %s\n", bridge->audio_bridge ? "connected" : "NULL");
    printf("  Speech: %s\n", bridge->speech_bridge ? "connected" : "NULL");
    printf("\n");

    printf("--- Binding State ---\n");
    printf("  Visual-Audio: %.3f%s\n",
           bridge->binding_state.binding_visual_audio,
           bridge->binding_state.visual_audio_bound ? " (BOUND)" : "");
    printf("  Visual-Speech: %.3f%s\n",
           bridge->binding_state.binding_visual_speech,
           bridge->binding_state.visual_speech_bound ? " (BOUND)" : "");
    printf("  Audio-Speech: %.3f%s\n",
           bridge->binding_state.binding_audio_speech,
           bridge->binding_state.audio_speech_bound ? " (BOUND)" : "");
    printf("  Overall Coherence: %.3f\n", bridge->binding_state.overall_coherence);
    printf("  Fully Bound: %s\n", bridge->binding_state.fully_bound ? "yes" : "no");
    printf("\n");

    printf("--- Modal Weights ---\n");
    printf("  Visual: %.3f\n", bridge->modal_weights.visual_weight);
    printf("  Audio: %.3f\n", bridge->modal_weights.audio_weight);
    printf("  Speech: %.3f\n", bridge->modal_weights.speech_weight);
    printf("\n");

    printf("--- Unified Quaternion ---\n");
    printf("  w (consolidation): %.3f\n", bridge->unified_quaternion.w);
    printf("  x (emotion): %.3f\n", bridge->unified_quaternion.x);
    printf("  y (salience): %.3f\n", bridge->unified_quaternion.y);
    printf("  z (accessibility): %.3f\n", bridge->unified_quaternion.z);
    printf("\n");

    printf("--- Dominant Modality ---\n");
    printf("  %s\n", pr_omni_bridge_dominant_to_string(bridge->dominant));
    printf("\n");

    printf("--- Configuration ---\n");
    printf("  Binding threshold: %.3f\n", bridge->config.binding_threshold);
    printf("  Fusion strategy: %s\n",
           pr_omni_bridge_fusion_to_string(bridge->config.fusion_strategy));
    printf("  Kuramoto coupling: %.3f\n", bridge->config.kuramoto_coupling);
    printf("  Phase gating: %s\n", bridge->config.enable_phase_gating ? "enabled" : "disabled");
    printf("\n");

    if (bridge->config.track_statistics) {
        printf("--- Statistics ---\n");
        printf("  Total updates: %lu\n", (unsigned long)bridge->stats.total_updates);
        printf("  Memories created: %lu\n", (unsigned long)bridge->stats.memories_created);
        printf("  Cross-modal retrievals: %lu\n",
               (unsigned long)bridge->stats.cross_modal_retrievals);
        printf("  Full bindings: %lu\n", (unsigned long)bridge->stats.full_bindings);
        printf("  Avg coherence: %.3f\n", bridge->stats.avg_overall_coherence);
        printf("  Avg binding: %.3f\n", bridge->stats.avg_binding_strength);
        printf("\n");
    }

    printf("====================================\n");
}

bool pr_omni_bridge_is_connected(const pr_omni_bridge_t* bridge) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_is_connected", 0.0f);


    return bridge->bridges_connected;
}

uint64_t pr_omni_bridge_current_time_ns(void) {
    /* Phase 8: Heartbeat at operation start */
    pr_omni_bridge_heartbeat("pr_omni_brid_current_time_ns", 0.0f);


    return get_time_ns();
}

//=============================================================================
// Instance Health Agent Setter (B25 Upgrade)
//=============================================================================

void pr_omni_bridge_set_instance_health_agent(
    pr_omni_bridge_t* bridge, nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// Training Hook Stubs (B25 Upgrade)
//=============================================================================

int pr_omni_bridge_training_begin(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_omni_bridge_training_begin: NULL argument");
        return -1;
    }
    pr_omni_bridge_heartbeat_instance(bridge->health_agent, "pr_omni_bridge_training_begin", 0.0f);
    return 0;
}

int pr_omni_bridge_training_end(pr_omni_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_omni_bridge_training_end: NULL argument");
        return -1;
    }
    pr_omni_bridge_heartbeat_instance(bridge->health_agent, "pr_omni_bridge_training_end", 1.0f);
    return 0;
}

int pr_omni_bridge_training_step(pr_omni_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "pr_omni_bridge_training_step: NULL argument");
        return -1;
    }
    pr_omni_bridge_heartbeat_instance(bridge->health_agent, "pr_omni_bridge_training_step", progress);
    return 0;
}
