/**
 * @file nimcp_mirror_attention_bridge.c
 * @brief Mirror Neuron - Attention System Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2025-12-31
 *
 * SIMD-optimized implementation of mirror-attention bridge for joint attention
 * and gaze following mechanisms.
 */

#include "cognitive/mirror_neurons/nimcp_mirror_attention_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/tensor/nimcp_tensor_simd.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_learning_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_attention_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_attention_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_attention_bridge_mesh_registry = NULL;

nimcp_error_t mirror_attention_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_attention_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_attention_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_attention_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_attention_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_attention_bridge_mesh_registry = registry;
    return err;
}

void mirror_attention_bridge_mesh_unregister(void) {
    if (g_mirror_attention_bridge_mesh_registry && g_mirror_attention_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_attention_bridge_mesh_registry, g_mirror_attention_bridge_mesh_id);
        g_mirror_attention_bridge_mesh_id = 0;
        g_mirror_attention_bridge_mesh_registry = NULL;
    }
}


#define LOG_MODULE "MIRROR_ATTENTION_BRIDGE"

static inline void mirror_attention_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_attention_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_attention_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


//=============================================================================
// Internal Structures
//=============================================================================

struct mirror_attention_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    mirror_attention_config_t config;
    mirror_attention_state_t state;

    /* Agent tracking */
    mirror_attention_agent_t agents[MIRROR_ATTENTION_MAX_AGENTS];
    uint32_t active_agent_count;

    /* Current attention state */
    mirror_saliency_state_t saliency;

    /* Statistics */
    mirror_attention_stats_t stats;

    /* Bio-async */
    bool bio_async_registered;

    /* Instance-level health agent (B22) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS(mirror_attention_bridge)

//=============================================================================
// Vector Math Helpers
//=============================================================================

static inline float vec3_dot(const mirror_attention_vec3_t* a,
                             const mirror_attention_vec3_t* b) {
    return a->x * b->x + a->y * b->y + a->z * b->z;
}

static inline float vec3_length(const mirror_attention_vec3_t* v) {
    return sqrtf(v->x * v->x + v->y * v->y + v->z * v->z);
}

static inline void vec3_normalize(mirror_attention_vec3_t* v) {
    float len = vec3_length(v);
    if (len > 1e-6f) {
        v->x /= len;
        v->y /= len;
        v->z /= len;
    }
}

static inline void vec3_scale(mirror_attention_vec3_t* v, float s) {
    v->x *= s;
    v->y *= s;
    v->z *= s;
}

static inline void vec3_add(mirror_attention_vec3_t* result,
                            const mirror_attention_vec3_t* a,
                            const mirror_attention_vec3_t* b) {
    result->x = a->x + b->x;
    result->y = a->y + b->y;
    result->z = a->z + b->z;
}

static inline float vec3_distance(const mirror_attention_vec3_t* a,
                                   const mirror_attention_vec3_t* b) {
    float dx = a->x - b->x;
    float dy = a->y - b->y;
    float dz = a->z - b->z;
    return sqrtf(dx * dx + dy * dy + dz * dz);
}

//=============================================================================
// Internal Functions
//=============================================================================

/**
 * @brief Get or create agent slot
 */
static mirror_attention_agent_t* get_or_create_agent(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
) {
    /* Find existing */
    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            return &bridge->agents[i];
        }
    }

    /* Find empty slot */
    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (!bridge->agents[i].active) {
            memset(&bridge->agents[i], 0, sizeof(mirror_attention_agent_t));
            bridge->agents[i].agent_id = agent_id;
            bridge->agents[i].active = true;
            bridge->agents[i].gaze_validity = 0.5f;  /* Start neutral */
            bridge->agents[i].gaze_following_rate = 0.0f;
            bridge->agents[i].joint_attention_tendency = 0.5f;
            bridge->active_agent_count++;
            return &bridge->agents[i];
        }
    }

    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "get_or_create_agent: operation failed");
    return NULL;
}

/**
 * @brief Compute cue strength based on type and observation
 */
static float compute_cue_strength(
    const mirror_attention_config_t* config,
    const mirror_gaze_observation_t* obs
) {
    float base_strength = obs->confidence;

    /* Weight by cue type */
    float type_weight = 1.0f;
    switch (obs->cue_type) {
        case MIRROR_CUE_GAZE:
            type_weight = config->gaze_cue_strength;
            break;
        case MIRROR_CUE_HEAD_TURN:
            type_weight = config->gaze_cue_strength * 0.9f;
            break;
        case MIRROR_CUE_POINTING:
            type_weight = config->pointing_cue_strength;
            break;
        case MIRROR_CUE_REACH:
            type_weight = config->pointing_cue_strength * 0.8f;
            break;
        case MIRROR_CUE_BODY_ORIENT:
            type_weight = config->gaze_cue_strength * 0.7f;
            break;
        default:
            type_weight = 0.5f;
            break;
    }

    /* Boost for duration (sustained attention is stronger cue) */
    float duration_factor = 1.0f;
    if (obs->duration_ms > 500.0f) {
        duration_factor = 1.0f + fminf(0.5f, (obs->duration_ms - 500.0f) / 2000.0f);
    }

    /* Boost for referential intent */
    if (obs->is_referential) {
        duration_factor *= 1.3f;
    }

    return fminf(1.0f, base_strength * type_weight * duration_factor);
}

/**
 * @brief Determine recommended attention shift type
 */
static attention_shift_type_t determine_shift_type(
    const mirror_attention_config_t* config,
    const mirror_gaze_observation_t* obs,
    mirror_attention_agent_t* agent,
    float cue_strength
) {
    if (cue_strength < config->cue_validity_threshold) {
        return ATTENTION_SHIFT_NONE;
    }

    /* Check agent validity history */
    if (agent && agent->gaze_validity < 0.3f) {
        return ATTENTION_SHIFT_SUPPRESSED;  /* Unreliable agent */
    }

    /* Short duration = reflexive, long = volitional */
    if (obs->duration_ms < config->reflexive_soa_ms) {
        return ATTENTION_SHIFT_REFLEXIVE;
    }

    return ATTENTION_SHIFT_VOLITIONAL;
}

/**
 * @brief Update saliency map with Gaussian
 */
static void update_saliency_gaussian(
    mirror_saliency_state_t* saliency,
    float focus_x,
    float focus_y,
    float sigma,
    float strength
) {
    float sigma_sq_2 = 2.0f * sigma * sigma;

    for (uint32_t i = 0; i < MIRROR_ATTENTION_SALIENCY_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_SALIENCY_SIZE > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_SALIENCY_SIZE);
        }

        float y = (float)i / (float)(MIRROR_ATTENTION_SALIENCY_SIZE - 1);
        float dy = y - focus_y;

        for (uint32_t j = 0; j < MIRROR_ATTENTION_SALIENCY_SIZE; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && MIRROR_ATTENTION_SALIENCY_SIZE > 256) {
                mirror_attention_bridge_heartbeat("mirror_atten_loop",
                                 (float)(j + 1) / (float)MIRROR_ATTENTION_SALIENCY_SIZE);
            }

            float x = (float)j / (float)(MIRROR_ATTENTION_SALIENCY_SIZE - 1);
            float dx = x - focus_x;

            float dist_sq = dx * dx + dy * dy;
            float gauss = strength * expf(-dist_sq / sigma_sq_2);

            saliency->saliency_boost[i][j] =
                fmaxf(saliency->saliency_boost[i][j], gauss);
        }
    }

    saliency->last_update_us = nimcp_time_now_us();
}

/**
 * @brief Decay saliency map
 */
static void decay_saliency(mirror_saliency_state_t* saliency, float decay_rate) {
    for (uint32_t i = 0; i < MIRROR_ATTENTION_SALIENCY_SIZE; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_SALIENCY_SIZE > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_SALIENCY_SIZE);
        }

        for (uint32_t j = 0; j < MIRROR_ATTENTION_SALIENCY_SIZE; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && MIRROR_ATTENTION_SALIENCY_SIZE > 256) {
                mirror_attention_bridge_heartbeat("mirror_atten_loop",
                                 (float)(j + 1) / (float)MIRROR_ATTENTION_SALIENCY_SIZE);
            }

            saliency->saliency_boost[i][j] *= (1.0f - decay_rate);
        }
    }
}

//=============================================================================
// Public API Implementation
//=============================================================================

mirror_attention_config_t mirror_attention_config_default(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_con", 0.0f);


    mirror_attention_config_t config = {
        .cue_validity_threshold = 0.3f,
        .reflexive_soa_ms = 150.0f,
        .voluntary_soa_ms = 300.0f,
        .gaze_cue_strength = 0.8f,
        .pointing_cue_strength = 0.9f,

        .joint_attention_threshold = 0.6f,
        .joint_attention_timeout_ms = 5000.0f,
        .enable_joint_attention_initiation = true,
        .enable_referential_gaze = true,

        .attention_mirror_gain = 0.3f,
        .mirror_attention_gain = 0.5f,

        .enable_saliency_modulation = true,
        .saliency_decay_rate = 0.02f,

        .enable_simd = true,
        .bio_async_enabled = true
    };
    return config;
}

mirror_attention_bridge_t* mirror_attention_create(
    const mirror_attention_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_cre", 0.0f);


    mirror_attention_bridge_t* bridge = nimcp_calloc(1,
                                                      sizeof(mirror_attention_bridge_t));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-Attention: Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_attention_create: bridge is NULL");
        return NULL;
    }

    /* Configuration */
    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = mirror_attention_config_default();
    }

    /* Initialize state */
    bridge->state = MIRROR_ATTENTION_STATE_IDLE;
    bridge->active_agent_count = 0;

    /* Initialize saliency */
    memset(&bridge->saliency, 0, sizeof(mirror_saliency_state_t));
    bridge->saliency.attention_sigma = 0.1f;
    bridge->saliency.attention_strength = 1.0f;

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_attention") != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-Attention: Failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_attention_create: validation failed");
        return NULL;
    }

    /* Register bio-async */
    if (bridge->config.bio_async_enabled) {
        mirror_attention_register_bio_async(bridge);
    }

    nimcp_log(LOG_LEVEL_INFO, "Mirror-Attention: Created bridge (joint_attention=%s)",
              bridge->config.enable_joint_attention_initiation ? "enabled" : "disabled");

    return bridge;
}

void mirror_attention_destroy(mirror_attention_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_attention");

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_des", 0.0f);


    if (bridge->bio_async_registered) {
        mirror_attention_unregister_bio_async(bridge);
    }

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);

    nimcp_free(bridge);
    bridge = NULL;
    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-Attention: Destroyed bridge");
}

bool mirror_attention_process_gaze(
    mirror_attention_bridge_t* bridge,
    const mirror_gaze_observation_t* observation,
    mirror_attention_cue_t* cue
) {
    if (!bridge || !observation || !cue) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_process_gaze: required parameter is NULL");
        return false;
    }
    BRIDGE_BBB_VALIDATE(bridge, observation, sizeof(*observation));

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_pro", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    memset(cue, 0, sizeof(mirror_attention_cue_t));
    cue->timestamp_us = nimcp_time_now_us();

    /* Get or create agent */
    mirror_attention_agent_t* agent = get_or_create_agent(bridge,
                                                          observation->agent_id);

    /* Compute cue strength */
    float cue_strength = compute_cue_strength(&bridge->config, observation);
    cue->cue_strength = cue_strength;

    /* Determine shift type */
    cue->recommended_shift = determine_shift_type(&bridge->config, observation,
                                                   agent, cue_strength);

    /* Set cue location */
    if (observation->target_position_valid) {
        cue->cue_location = observation->target_position;
    } else {
        /* Compute from gaze direction */
        float distance = mirror_attention_compute_gaze_target(
            &observation->agent_position,
            &observation->direction,
            &cue->cue_location
        );
        if (distance < 0) {
            /* Default to some distance along gaze */
            cue->cue_location.x = observation->agent_position.x +
                                  observation->direction.x * 2.0f;
            cue->cue_location.y = observation->agent_position.y +
                                  observation->direction.y * 2.0f;
            cue->cue_location.z = observation->agent_position.z +
                                  observation->direction.z * 2.0f;
        }
    }

    /* Object cuing */
    if (observation->target_object_valid) {
        cue->cue_object_id = observation->target_object_id;
        cue->object_cued = true;
    }

    /* Compute validity (learned from history) */
    cue->cue_validity = agent ? agent->gaze_validity : 0.5f;

    /* Expected SOA */
    if (cue->recommended_shift == ATTENTION_SHIFT_REFLEXIVE) {
        cue->expected_soa_ms = bridge->config.reflexive_soa_ms;
    } else if (cue->recommended_shift == ATTENTION_SHIFT_VOLITIONAL) {
        cue->expected_soa_ms = bridge->config.voluntary_soa_ms;
    }

    /* Handle mutual gaze */
    if (observation->is_mutual_gaze) {
        bridge->stats.mutual_gaze_events++;
        /* Mutual gaze is special - may initiate joint attention */
        if (agent && agent->joint_state == JOINT_ATTENTION_NONE) {
            agent->joint_state = JOINT_ATTENTION_RESPONDING;
            agent->joint_attention_start_us = cue->timestamp_us;
        }
    }

    /* Update joint attention state */
    if (agent) {
        cue->joint_state = agent->joint_state;
        cue->joint_agent_id = agent->agent_id;

        /* Update agent tracking */
        agent->last_gaze_direction = observation->direction;
        agent->last_target = cue->cue_location;
        agent->last_cue_timestamp_us = cue->timestamp_us;
    }

    /* Update saliency map if enabled */
    if (bridge->config.enable_saliency_modulation &&
        cue->recommended_shift != ATTENTION_SHIFT_NONE) {
        /* Convert 3D location to 2D normalized for saliency map */
        /* Simple projection - assume Y is depth */
        float sal_x = fminf(1.0f, fmaxf(0.0f, (cue->cue_location.x + 2.0f) / 4.0f));
        float sal_y = fminf(1.0f, fmaxf(0.0f, (cue->cue_location.z + 2.0f) / 4.0f));

        update_saliency_gaussian(&bridge->saliency, sal_x, sal_y,
                                 0.1f, cue_strength);
    }

    /* Update statistics */
    if (observation->cue_type == MIRROR_CUE_GAZE ||
        observation->cue_type == MIRROR_CUE_HEAD_TURN) {
        bridge->stats.gaze_cues_detected++;
    } else if (observation->cue_type == MIRROR_CUE_POINTING ||
               observation->cue_type == MIRROR_CUE_REACH) {
        bridge->stats.pointing_cues_detected++;
    }

    if (cue->recommended_shift != ATTENTION_SHIFT_NONE &&
        cue->recommended_shift != ATTENTION_SHIFT_SUPPRESSED) {
        bridge->stats.attention_shifts_triggered++;
    }

    bridge->stats.avg_cue_strength =
        bridge->stats.avg_cue_strength * 0.95f + cue_strength * 0.05f;
    bridge->stats.avg_gaze_validity =
        bridge->stats.avg_gaze_validity * 0.95f + cue->cue_validity * 0.05f;

    bridge->state = MIRROR_ATTENTION_STATE_CUE_DETECTED;

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Bio-async message sending would happen here when integrated
     * into a full bio-async context with proper router registration */
    (void)bridge->bio_async_registered; /* Suppress unused warning */

    return true;
}

float mirror_attention_compute_gaze_target(
    const mirror_attention_vec3_t* agent_position,
    const mirror_attention_vec3_t* gaze_direction,
    mirror_attention_vec3_t* target
) {
    if (!agent_position || !gaze_direction || !target) return -1.0f;

    /* Simple ray-plane intersection with ground plane (y=0) */
    /* P = O + t*D where P.y = 0 */
    /* t = -O.y / D.y */

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_com", 0.0f);


    float t = 0.0f;
    if (fabsf(gaze_direction->y) < 1e-6f) {
        /* Horizontal gaze - use default distance of 3.0m */
        t = 3.0f;
    } else {
        t = -agent_position->y / gaze_direction->y;
        if (t < 0.0f) {
            /* Looking up, use arbitrary distance */
            t = 3.0f;
        }
    }

    target->x = agent_position->x + gaze_direction->x * t;
    target->y = agent_position->y + gaze_direction->y * t;
    target->z = agent_position->z + gaze_direction->z * t;

    return t;
}

uint32_t mirror_attention_process_batch(
    mirror_attention_bridge_t* bridge,
    const mirror_gaze_observation_t* observations,
    mirror_attention_cue_t* cues,
    uint32_t count
) {
    if (!bridge || !observations || !cues || count == 0) {
        if (!bridge || !observations || !cues) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_process_batch: required parameter is NULL");
            return 0;
        }
        return 0;
    }
    BRIDGE_BBB_VALIDATE(bridge, observations, count * sizeof(*observations));

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_pro", 0.0f);


    uint32_t processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)count);
        }

        if (mirror_attention_process_gaze(bridge, &observations[i], &cues[i])) {
            processed++;
        }
    }

    if (bridge->config.enable_simd && count >= MIRROR_ATTENTION_SIMD_THRESHOLD) {
        bridge->stats.simd_operations++;
    }

    return processed;
}

joint_attention_state_t mirror_attention_get_joint_state(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_get_joint_state: bridge is NULL");
        return JOINT_ATTENTION_NONE;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    joint_attention_state_t state = JOINT_ATTENTION_NONE;
    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            state = bridge->agents[i].joint_state;
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return state;
}

bool mirror_attention_initiate_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id,
    const mirror_attention_vec3_t* target
) {
    if (!bridge || !target) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_initiate_joint: required parameter is NULL");
        return false;
    }
    if (!bridge->config.enable_joint_attention_initiation) return false;

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_ini", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mirror_attention_agent_t* agent = get_or_create_agent(bridge, agent_id);
    if (!agent) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "mirror_attention_initiate_joint: agent is NULL");
        return false;
    }

    /* Can only initiate from NONE state */
    if (agent->joint_state != JOINT_ATTENTION_NONE) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_initiate_joint: validation failed");
        return false;
    }

    agent->joint_state = JOINT_ATTENTION_INITIATING;
    agent->last_target = *target;
    agent->joint_attention_start_us = nimcp_time_now_us();

    bridge->state = MIRROR_ATTENTION_STATE_JOINT;

    nimcp_mutex_unlock(bridge->base.mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-Attention: Initiated joint attention with agent %u", agent_id);

    return true;
}

bool mirror_attention_respond_to_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_respond_to_joint: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_res", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    mirror_attention_agent_t* agent = NULL;
    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            agent = &bridge->agents[i];
            break;
        }
    }

    if (!agent || agent->joint_state != JOINT_ATTENTION_RESPONDING) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_respond_to_joint: agent is NULL");
        return false;
    }

    agent->joint_state = JOINT_ATTENTION_ESTABLISHED;
    agent->successful_joint_attention_count++;

    bridge->stats.joint_attention_episodes++;
    bridge->state = MIRROR_ATTENTION_STATE_JOINT;

    nimcp_mutex_unlock(bridge->base.mutex);

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-Attention: Joint attention established with agent %u", agent_id);

    return true;
}

void mirror_attention_break_joint(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_break_joint: bridge is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_bre", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            mirror_attention_agent_t* agent = &bridge->agents[i];

            /* Calculate duration */
            if (agent->joint_state == JOINT_ATTENTION_ESTABLISHED) {
                uint64_t now = nimcp_time_now_us();
                float duration_ms = (float)(now - agent->joint_attention_start_us) / 1000.0f;
                agent->avg_joint_duration_ms =
                    agent->avg_joint_duration_ms * 0.8f + duration_ms * 0.2f;
            }

            agent->joint_state = JOINT_ATTENTION_BREAKING;
            break;
        }
    }

    bridge->state = MIRROR_ATTENTION_STATE_INDEPENDENT;

    nimcp_mutex_unlock(bridge->base.mutex);
}

float mirror_attention_get_sensitivity_at(
    mirror_attention_bridge_t* bridge,
    const mirror_attention_vec3_t* position
) {
    if (!bridge || !position) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_get_sensitivity_at: required parameter is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Compute distance from attention focus */
    float dist = vec3_distance(&bridge->saliency.attention_focus, position);

    /* Gaussian falloff */
    float sigma = bridge->saliency.attention_sigma;
    float sensitivity = expf(-(dist * dist) / (2.0f * sigma * sigma));

    /* Apply mirror-attention gain */
    sensitivity = 1.0f + sensitivity * bridge->config.attention_mirror_gain;

    nimcp_mutex_unlock(bridge->base.mutex);
    return sensitivity;
}

void mirror_attention_set_focus(
    mirror_attention_bridge_t* bridge,
    const mirror_attention_vec3_t* focus,
    float strength,
    float sigma
) {
    if (!bridge || !focus) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_set_focus: required parameter is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_set", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->saliency.attention_focus = *focus;
    bridge->saliency.attention_strength = strength;
    bridge->saliency.attention_sigma = sigma;

    nimcp_mutex_unlock(bridge->base.mutex);
}

float mirror_attention_get_saliency_boost(
    mirror_attention_bridge_t* bridge,
    float x,
    float y
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_get_saliency_boost: bridge is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay saliency */
    decay_saliency(&bridge->saliency, bridge->config.saliency_decay_rate);

    /* Get saliency at location */
    uint32_t ix = (uint32_t)(x * (MIRROR_ATTENTION_SALIENCY_SIZE - 1));
    uint32_t iy = (uint32_t)(y * (MIRROR_ATTENTION_SALIENCY_SIZE - 1));

    ix = (ix < MIRROR_ATTENTION_SALIENCY_SIZE) ? ix : MIRROR_ATTENTION_SALIENCY_SIZE - 1;
    iy = (iy < MIRROR_ATTENTION_SALIENCY_SIZE) ? iy : MIRROR_ATTENTION_SALIENCY_SIZE - 1;

    float boost = 1.0f + bridge->saliency.saliency_boost[iy][ix];

    nimcp_mutex_unlock(bridge->base.mutex);
    return boost;
}

void mirror_attention_simd_gaze_targets(
    const float* positions,
    const float* directions,
    float* targets,
    float* distances,
    uint32_t count
) {
    /* SIMD batch gaze target computation */
    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_sim", 0.0f);


    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)count);
        }

        mirror_attention_vec3_t pos = {
            positions[i * 3 + 0],
            positions[i * 3 + 1],
            positions[i * 3 + 2]
        };
        mirror_attention_vec3_t dir = {
            directions[i * 3 + 0],
            directions[i * 3 + 1],
            directions[i * 3 + 2]
        };
        mirror_attention_vec3_t target;

        distances[i] = mirror_attention_compute_gaze_target(&pos, &dir, &target);

        targets[i * 3 + 0] = target.x;
        targets[i * 3 + 1] = target.y;
        targets[i * 3 + 2] = target.z;
    }
}

void mirror_attention_simd_update_saliency(
    float* saliency_map,
    uint32_t size,
    float focus_x,
    float focus_y,
    float sigma,
    float strength
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_sim", 0.0f);


    if (size < 2) return;  /* Need at least 2 elements for saliency normalization */

    float sigma_sq_2 = 2.0f * sigma * sigma;

    for (uint32_t i = 0; i < size; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && size > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)size);
        }

        float y = (float)i / (float)(size - 1);
        float dy = y - focus_y;

        for (uint32_t j = 0; j < size; j++) {
            /* Phase 8: Loop progress heartbeat */
            if ((j & 0xFF) == 0 && size > 256) {
                mirror_attention_bridge_heartbeat("mirror_atten_loop",
                                 (float)(j + 1) / (float)size);
            }

            float x = (float)j / (float)(size - 1);
            float dx = x - focus_x;

            float dist_sq = dx * dx + dy * dy;
            float gauss = strength * expf(-dist_sq / sigma_sq_2);

            saliency_map[i * size + j] =
                fmaxf(saliency_map[i * size + j], gauss);
        }
    }
}

mirror_attention_agent_t* mirror_attention_get_agent(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return NULL;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_get", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    mirror_attention_agent_t* agent = get_or_create_agent(bridge, agent_id);
    nimcp_mutex_unlock(bridge->base.mutex);

    return agent;
}

void mirror_attention_update_validity(
    mirror_attention_bridge_t* bridge,
    uint32_t agent_id,
    bool valid
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_update_validity: bridge is NULL");
        return;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_upd", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active && bridge->agents[i].agent_id == agent_id) {
            mirror_attention_agent_t* agent = &bridge->agents[i];

            /* Update validity with learning rate */
            float lr = NIMCP_LEARNING_RATE_COARSE;
            float target = valid ? 1.0f : 0.0f;
            agent->gaze_validity = agent->gaze_validity * (1.0f - lr) + target * lr;

            /* Update following rate */
            if (valid) {
                agent->gaze_following_rate =
                    agent->gaze_following_rate * 0.9f + 0.1f;
            }
            break;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
}

bool mirror_attention_register_bio_async(mirror_attention_bridge_t* bridge) {
    if (!bridge || bridge->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_register_bio_async: bridge is NULL");
        return false;
    }

    /* Mark as registered - actual router registration would happen
     * when integrated into a full bio-async context */
    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_reg", 0.0f);


    bridge->bio_async_registered = true;
    return true;
}

void mirror_attention_unregister_bio_async(mirror_attention_bridge_t* bridge) {
    if (!bridge || !bridge->bio_async_registered) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_unr", 0.0f);


    bridge->bio_async_registered = false;
}

bool mirror_attention_get_stats(
    const mirror_attention_bridge_t* bridge,
    mirror_attention_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_attention_get_stats: required parameter is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_get", 0.0f);


    nimcp_mutex_lock(((mirror_attention_bridge_t*)bridge)->base.mutex);
    *stats = bridge->stats;

    /* Count active agents */
    stats->active_agents = 0;
    uint64_t total_success = 0;
    uint64_t total_attempts = 0;
    float total_duration = 0.0f;
    uint32_t duration_count = 0;

    for (uint32_t i = 0; i < MIRROR_ATTENTION_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_ATTENTION_MAX_AGENTS > 256) {
            mirror_attention_bridge_heartbeat("mirror_atten_loop",
                             (float)(i + 1) / (float)MIRROR_ATTENTION_MAX_AGENTS);
        }

        if (bridge->agents[i].active) {
            stats->active_agents++;
            total_success += bridge->agents[i].successful_joint_attention_count;
            total_attempts += bridge->agents[i].successful_joint_attention_count +
                              bridge->agents[i].failed_joint_attention_count;
            if (bridge->agents[i].avg_joint_duration_ms > 0.0f) {
                total_duration += bridge->agents[i].avg_joint_duration_ms;
                duration_count++;
            }
        }
    }

    if (total_attempts > 0) {
        stats->successful_joint_rate = (float)total_success / (float)total_attempts;
    }
    if (duration_count > 0) {
        stats->avg_joint_attention_duration_ms = total_duration / (float)duration_count;
    }

    nimcp_mutex_unlock(((mirror_attention_bridge_t*)bridge)->base.mutex);
    return true;
}

void mirror_attention_reset_stats(mirror_attention_bridge_t* bridge) {
    if (!bridge) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_attention_bridge_heartbeat("mirror_atten_mirror_attention_res", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mirror_attention_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);
}

//=============================================================================
// B22: Instance Health Agent Setter
//=============================================================================

void mirror_attention_bridge_set_instance_health_agent(
    mirror_attention_bridge_t* bridge,
    nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

//=============================================================================
// B22: Training Hook Stubs
//=============================================================================

int mirror_attention_bridge_training_begin(mirror_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_attention_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_attention_bridge_heartbeat("mirror_atten_training_begin", 0.0f);
    return 0;
}

int mirror_attention_bridge_training_end(mirror_attention_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_attention_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_attention_bridge_heartbeat("mirror_atten_training_end", 1.0f);
    return 0;
}

int mirror_attention_bridge_training_step(mirror_attention_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_attention_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_attention_bridge_heartbeat_instance(bridge->health_agent, "mirror_attention_bridge_training_step", progress);
    return 0;
}
