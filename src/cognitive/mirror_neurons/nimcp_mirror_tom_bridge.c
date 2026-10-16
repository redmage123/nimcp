/**
 * @file nimcp_mirror_tom_bridge.c
 * @brief Mirror Neurons - Theory of Mind Bidirectional Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 *
 * WHAT: Bidirectional integration between mirror neurons and Theory of Mind
 * WHY:  Mirror neurons provide embodied simulation that grounds mental state inference
 * HOW:  SIMD-optimized similarity computations for efficient intention/belief matching
 */

#include "cognitive/mirror_neurons/nimcp_mirror_tom_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/time/nimcp_time.h"
#include "utils/tensor/nimcp_tensor_simd.h"
#include "utils/containers/nimcp_vector.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include "async/nimcp_bio_messages.h"
#include <string.h>
#include <math.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_tom_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_tom_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_tom_bridge_mesh_registry = NULL;

nimcp_error_t mirror_tom_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_tom_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_tom_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_tom_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_tom_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_tom_bridge_mesh_registry = registry;
    return err;
}

void mirror_tom_bridge_mesh_unregister(void) {
    if (g_mirror_tom_bridge_mesh_registry && g_mirror_tom_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_tom_bridge_mesh_registry, g_mirror_tom_bridge_mesh_id);
        g_mirror_tom_bridge_mesh_id = 0;
        g_mirror_tom_bridge_mesh_registry = NULL;
    }
}


#define LOG_MODULE "MIRROR_TOM_BRIDGE"

static inline void mirror_tom_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_tom_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_tom_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#include "constants/nimcp_constants.h"
#define EPSILON                           NIMCP_EPSILON_NUMERICAL
#define AGENT_HASH_PRIME                  37
#define BELIEF_DECAY_TIME_CONSTANT_US     3600000000ULL  /* 1 hour */
#define INTENTION_HISTORY_SIZE            8
#define TRUST_MODULATION_WEIGHT           0.4f
#define ALIGNMENT_MODULATION_WEIGHT       0.3f
#define DISTANCE_MODULATION_WEIGHT        0.2f
#define COMPETENCE_MODULATION_WEIGHT      0.1f

/* ============================================================================
 * Internal Data Structures
 * ============================================================================ */

/**
 * @brief Internal bridge structure
 */
struct mirror_tom_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */

    /* Configuration */
    mirror_tom_config_t config;

    /* Agent tracking */
    mirror_tom_agent_state_t agents[MIRROR_TOM_MAX_AGENTS];
    uint32_t agent_count;

    /* Connected systems */
    mirror_neurons_t mirror;
    theory_of_mind_t tom;

    /* Current effects */
    mirror_to_tom_effects_t mirror_effects;
    tom_to_mirror_effects_t tom_effects;

    /* Statistics */
    mirror_tom_stats_t stats;

    /* Bio-async registration */
    bool bio_async_registered;
    uint32_t handler_id;

    /* Instance-level health agent (B22) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(mirror_tom_bridge, struct mirror_tom_bridge)

/* ============================================================================
 * SIMD Helper Functions
 * ============================================================================ */

/**
 * @brief SIMD-optimized sum of squares
 */
static double simd_sum_sq(const float* v, uint32_t dim) {
    if (dim >= MIRROR_TOM_SIMD_BATCH_THRESHOLD) {
        return tensor_simd_sum_sq_f32(v, dim);
    }
    /* Scalar fallback */
    double sum = 0.0;
    for (uint32_t i = 0; i < dim; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && dim > 256) {
            mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                             (float)(i + 1) / (float)dim);
        }

        sum += (double)v[i] * (double)v[i];
    }
    return sum;
}

/* Cosine similarity uses nimcp_vector_cosine_similarity from utils/containers/nimcp_vector.h */

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/**
 * @brief Hash agent ID to slot index
 */
static uint32_t hash_agent_id(uint32_t agent_id) {
    return (agent_id * AGENT_HASH_PRIME) % MIRROR_TOM_MAX_AGENTS;
}

/**
 * @brief Find agent slot (returns -1 if not found)
 */
static int32_t find_agent_slot(const struct mirror_tom_bridge* bridge,
                                uint32_t agent_id) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint32_t start = hash_agent_id(agent_id);
    for (uint32_t i = 0; i < MIRROR_TOM_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_TOM_MAX_AGENTS > 256) {
            mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                             (float)(i + 1) / (float)MIRROR_TOM_MAX_AGENTS);
        }

        uint32_t idx = (start + i) % MIRROR_TOM_MAX_AGENTS;
        if (bridge->agents[idx].is_active &&
            bridge->agents[idx].agent_id == agent_id) {
            return (int32_t)idx;
        }
    }
    return -1;
}

/**
 * @brief Find or create agent slot
 */
static int32_t find_or_create_agent_slot(struct mirror_tom_bridge* bridge,
                                          uint32_t agent_id) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* First check existing */
    int32_t existing = find_agent_slot(bridge, agent_id);
    if (existing >= 0) return existing;

    /* Check capacity */
    if (bridge->agent_count >= MIRROR_TOM_MAX_AGENTS) {
        nimcp_log(LOG_LEVEL_WARN, "Mirror-ToM: max agents (%d) reached",
                  MIRROR_TOM_MAX_AGENTS);
            return -1;
    }

    /* Find empty slot */
    uint32_t start = hash_agent_id(agent_id);
    for (uint32_t i = 0; i < MIRROR_TOM_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_TOM_MAX_AGENTS > 256) {
            mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                             (float)(i + 1) / (float)MIRROR_TOM_MAX_AGENTS);
        }

        uint32_t idx = (start + i) % MIRROR_TOM_MAX_AGENTS;
        if (!bridge->agents[idx].is_active) {
            /* Initialize agent */
            memset(&bridge->agents[idx], 0, sizeof(mirror_tom_agent_state_t));
            bridge->agents[idx].agent_id = agent_id;
            bridge->agents[idx].is_active = true;
            bridge->agents[idx].first_seen_us = nimcp_time_now_us();
            bridge->agents[idx].last_seen_us = bridge->agents[idx].first_seen_us;

            /* Initialize mental state defaults */
            bridge->agents[idx].mental_state.agent_id = agent_id;
            bridge->agents[idx].mental_state.trust_level = 0.5f;
            bridge->agents[idx].mental_state.competence_estimate = 0.5f;
            bridge->agents[idx].mental_state.social_distance = 0.5f;

            bridge->agent_count++;
            if (bridge->agent_count > bridge->stats.peak_agents) {
                bridge->stats.peak_agents = bridge->agent_count;
            }
            bridge->stats.agents_tracked = bridge->agent_count;

            return (int32_t)idx;
        }
    }
    return -1;
}

/**
 * @brief Compute resonance modulation from mental state
 */
static float compute_modulation_from_mental_state(
    const mirror_tom_mental_state_t* state,
    const mirror_tom_config_t* config) {

    if (!state || !config) return 1.0f;

    float modulation = 1.0f;

    /* Trust increases resonance */
    float trust_factor = (state->trust_level - 0.5f) * 2.0f;  /* Map 0-1 to -1 to 1 */
    modulation += trust_factor * TRUST_MODULATION_WEIGHT;

    /* Alignment increases resonance */
    modulation += state->intention_alignment * ALIGNMENT_MODULATION_WEIGHT;

    /* Social closeness increases resonance */
    float closeness = 1.0f - state->social_distance;
    modulation += (closeness - 0.5f) * DISTANCE_MODULATION_WEIGHT;

    /* Competence increases resonance (want to imitate competent agents) */
    float competence_factor = (state->competence_estimate - 0.5f) * 2.0f;
    modulation += competence_factor * COMPETENCE_MODULATION_WEIGHT;

    /* Deception dramatically reduces resonance */
    if (state->deception_likelihood > config->deception_suppress_threshold) {
        modulation *= 0.2f;
    }

    return nimcp_clampf(modulation, config->modulation_min, config->modulation_max);
}

/**
 * @brief Add intention to history ring buffer
 */
static void add_intention_to_history(mirror_tom_agent_state_t* agent,
                                      const float* intention,
                                      uint32_t dim) {
    if (!agent || !intention || dim == 0) return;

    /* Shift history */
    uint32_t idx = agent->intention_count % INTENTION_HISTORY_SIZE;

    /* Copy intention (truncate if needed) */
    uint32_t copy_dim = dim < MIRROR_TOM_INTENTION_DIM ? dim : MIRROR_TOM_INTENTION_DIM;
    memcpy(agent->intention_history[idx], intention, copy_dim * sizeof(float));

    if (agent->intention_count < INTENTION_HISTORY_SIZE) {
        agent->intention_count++;
    }

    /* Compute intention consistency using SIMD */
    if (agent->intention_count >= 2) {
        float total_sim = 0.0f;
        uint32_t comparisons = 0;
        for (uint32_t i = 0; i < agent->intention_count - 1; i++) {
            float sim = nimcp_vector_cosine_similarity(
                agent->intention_history[i],
                agent->intention_history[i + 1],
                copy_dim);
            total_sim += sim;
            comparisons++;
        }
        agent->intention_consistency = comparisons > 0 ? total_sim / comparisons : 0.5f;
    }
}

/* Forward declarations for bio-async registration */
bool mirror_tom_register_bio_async(mirror_tom_bridge_t bridge);
void mirror_tom_unregister_bio_async(mirror_tom_bridge_t bridge);

/* ============================================================================
 * Core API - Lifecycle Management
 * ============================================================================ */

mirror_tom_bridge_t mirror_tom_create(const mirror_tom_config_t* config) {
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_create", 0.0f);


    struct mirror_tom_bridge* bridge = nimcp_malloc(sizeof(struct mirror_tom_bridge));
    if (!bridge) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-ToM: failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    memset(bridge, 0, sizeof(struct mirror_tom_bridge));

    /* Apply configuration */
    bridge->config = config ? *config : mirror_tom_get_default_config();

    /* Initialize bridge base infrastructure (includes mutex) */
    if (bridge_base_init(&bridge->base, 0, "mirror_tom") != 0) {
        nimcp_log(LOG_LEVEL_ERROR, "Mirror-ToM: failed to initialize bridge base");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NOT_INITIALIZED, "mirror_tom_create: validation failed");
        return NULL;
    }

    /* Initialize effects to neutral */
    bridge->tom_effects.resonance_gain = 1.0f;
    bridge->tom_effects.imitation_gate = 1.0f;

    /* Register with bio-async if enabled */
    if (bridge->config.bio_async_enabled) {
        mirror_tom_register_bio_async(bridge);
    }

    nimcp_log(LOG_LEVEL_INFO, "Mirror-ToM bridge created (SIMD=%s)",
              bridge->config.enable_simd_optimization ? "enabled" : "disabled");

    NIMCP_LOGGING_INFO("Created %s bridge", "mirror_tom");
    return bridge;
}

void mirror_tom_destroy(mirror_tom_bridge_t bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "mirror_tom");

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_destroy", 0.0f);

    if (bridge->bio_async_registered) {
        mirror_tom_unregister_bio_async(bridge);
    }

    nimcp_log(LOG_LEVEL_INFO, "Mirror-ToM bridge destroyed (tracked %u agents, %u observations)",
              bridge->agent_count, bridge->stats.total_observations);

    /* Cleanup base bridge infrastructure */
    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
    bridge = NULL;
}

mirror_tom_config_t mirror_tom_get_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_defau", 0.0f);


    mirror_tom_config_t config;

    /* Mirror → ToM thresholds */
    config.action_intent_threshold = MIRROR_TOM_ACTION_INTENT_THRESHOLD;
    config.false_belief_pe_threshold = MIRROR_TOM_FALSE_BELIEF_PE_THRESHOLD;
    config.empathy_resonance_threshold = MIRROR_TOM_EMPATHY_RESONANCE_THRESHOLD;

    /* ToM → Mirror modulation */
    config.trust_resonance_threshold = MIRROR_TOM_TRUST_RESONANCE_THRESHOLD;
    config.deception_suppress_threshold = MIRROR_TOM_DECEPTION_SUPPRESS_THRESHOLD;
    config.modulation_min = MIRROR_TOM_MODULATION_MIN;
    config.modulation_max = MIRROR_TOM_MODULATION_MAX;

    /* Coupling */
    config.coupling_rate = MIRROR_TOM_COUPLING_RATE;
    config.belief_decay_rate = 0.001f;

    /* Features */
    config.enable_intention_inference = true;
    config.enable_empathy_pathway = true;
    config.enable_false_belief_detection = true;
    config.enable_deception_suppression = true;
    config.enable_simd_optimization = true;
    config.bio_async_enabled = true;

    return config;
}

int mirror_tom_connect_mirror(mirror_tom_bridge_t bridge, mirror_neurons_t mirror) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_connect_m", 0.0f);


    bridge->mirror = mirror;
    nimcp_log(LOG_LEVEL_INFO, "Mirror-ToM: connected to mirror neuron system");
    return 0;
}

int mirror_tom_connect_tom(mirror_tom_bridge_t bridge, theory_of_mind_t tom) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_connect_t", 0.0f);


    bridge->tom = tom;
    nimcp_log(LOG_LEVEL_INFO, "Mirror-ToM: connected to Theory of Mind system");
    return 0;
}

/* ============================================================================
 * Core API - Mirror → ToM Pathway
 * ============================================================================ */

int mirror_tom_process_observation(mirror_tom_bridge_t bridge,
                                    uint32_t agent_id,
                                    const mirror_tom_observation_t* observation) {
    if (!bridge || !observation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_process_observation: required parameter is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, observation, sizeof(*observation));

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_process_o", 0.0f);


    int32_t slot = find_or_create_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_connect_tom: validation failed");
        return -1;
    }

    mirror_tom_agent_state_t* agent = &bridge->agents[slot];
    agent->last_seen_us = observation->timestamp_us;
    agent->observation_count++;
    bridge->stats.total_observations++;

    /* Process intention inference if enabled */
    if (bridge->config.enable_intention_inference &&
        observation->action_dim > 0) {

        char intention_buf[NIMCP_LABEL_BUFFER_SIZE];
        float intent_conf = 0.0f;
        if (mirror_tom_infer_intention(bridge, agent_id,
                                        observation->action_features,
                                        observation->action_dim,
                                        intention_buf, &intent_conf) == 0) {
            bridge->stats.intentions_inferred++;
            bridge->mirror_effects.intention_inference_strength = intent_conf;
        }
    }

    /* Process empathy if enabled */
    if (bridge->config.enable_empathy_pathway &&
        observation->emotional_intensity > bridge->config.empathy_resonance_threshold) {

        mirror_tom_trigger_empathy(bridge, agent_id,
                                    observation->emotional_resonance,
                                    observation->emotional_intensity);
    }

    /* Process false belief detection if enabled */
    if (bridge->config.enable_false_belief_detection &&
        observation->prediction_error > bridge->config.false_belief_pe_threshold) {

        mirror_tom_signal_false_belief(bridge, agent_id,
                                        observation->prediction_error,
                                        NULL, 0);
    }

    /* Update action understanding depth */
    float understanding = observation->goal_confidence * 0.5f +
                          observation->resonance_strength * 0.3f +
                          (1.0f - observation->prediction_error) * 0.2f;
    bridge->mirror_effects.action_understanding_depth = understanding;

    return 0;
}

int mirror_tom_infer_intention(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                const float* action_features,
                                uint32_t action_dim,
                                char* out_intention,
                                float* out_confidence) {
    if (!bridge || !action_features || action_dim == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_infer_intention: required parameter is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, action_features, action_dim * sizeof(float));

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_infer_int", 0.0f);


    int32_t slot = find_or_create_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_connect_tom: validation failed");
        return -1;
    }

    mirror_tom_agent_state_t* agent = &bridge->agents[slot];

    /* Add to intention history */
    add_intention_to_history(agent, action_features, action_dim);

    /* Compute confidence based on consistency and action features magnitude */
    bool used_simd = false;
    float magnitude = (float)sqrt(simd_sum_sq(action_features, action_dim));
    float confidence = nimcp_clampf(magnitude * agent->intention_consistency, 0.0f, 1.0f);

    /* Track SIMD usage */
    if (action_dim >= MIRROR_TOM_SIMD_BATCH_THRESHOLD) {
        bridge->stats.simd_computations++;
    } else {
        bridge->stats.scalar_computations++;
    }

    /* Generate intention description based on goal and consistency */
    if (out_intention) {
        if (agent->mental_state.predicted_intention[0] != '\0' &&
            agent->intention_consistency > 0.7f) {
            snprintf(out_intention, 128, "Continuing: %s (consistency=%.2f)",
                     agent->mental_state.predicted_intention, agent->intention_consistency);
        } else {
            snprintf(out_intention, 128, "Goal-directed action (confidence=%.2f)",
                     confidence);
        }
    }

    if (out_confidence) *out_confidence = confidence;

    return 0;
}

int mirror_tom_trigger_empathy(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                mirror_tom_emotion_t emotion,
                                float intensity) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_trigger_e", 0.0f);


    int32_t slot = find_or_create_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_connect_tom: validation failed");
        return -1;
    }

    bridge->stats.empathy_activations++;

    /* Scale empathy by trust and social closeness */
    mirror_tom_agent_state_t* agent = &bridge->agents[slot];
    float empathy_strength = intensity * agent->mental_state.trust_level *
                             (1.0f - agent->mental_state.social_distance * 0.5f);

    bridge->mirror_effects.empathy_contribution = nimcp_clampf(empathy_strength, 0.0f, 1.0f);

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-ToM: empathy triggered for agent %u, "
              "emotion=%s, intensity=%.2f, strength=%.2f",
              agent_id, mirror_tom_emotion_name(emotion), intensity, empathy_strength);

    return 0;
}

int mirror_tom_signal_false_belief(mirror_tom_bridge_t bridge,
                                    uint32_t agent_id,
                                    float prediction_error,
                                    const float* context_features,
                                    uint32_t context_dim) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_signal_fa", 0.0f);


    (void)context_features;
    (void)context_dim;

    int32_t slot = find_or_create_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_connect_tom: validation failed");
        return -1;
    }

    mirror_tom_agent_state_t* agent = &bridge->agents[slot];
    agent->false_belief_count++;
    bridge->stats.false_beliefs_detected++;

    /* Update average prediction error */
    float alpha = 0.1f;  /* EMA smoothing */
    agent->avg_prediction_error = agent->avg_prediction_error * (1.0f - alpha) +
                                   prediction_error * alpha;

    /* Mark as potential false belief holder if error is high */
    agent->mental_state.is_false_belief_holder = prediction_error > 0.7f;

    /* Signal affects future observations */
    bridge->mirror_effects.false_belief_signal = prediction_error;

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-ToM: false belief signal for agent %u, PE=%.2f",
              agent_id, prediction_error);

    return 0;
}

/* ============================================================================
 * Core API - ToM → Mirror Pathway
 * ============================================================================ */

int mirror_tom_update_mental_state(mirror_tom_bridge_t bridge,
                                    const mirror_tom_mental_state_t* mental_state) {
    if (!bridge || !mental_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_update_mental_state: required parameter is NULL");
        return -1;
    }
    BRIDGE_BBB_VALIDATE(bridge, mental_state, sizeof(*mental_state));

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_update_me", 0.0f);


    int32_t slot = find_or_create_agent_slot(bridge, mental_state->agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    /* Update mental state */
    bridge->agents[slot].mental_state = *mental_state;

    /* Recompute resonance gain */
    float gain = compute_modulation_from_mental_state(mental_state, &bridge->config);
    bridge->tom_effects.resonance_gain = gain;
    bridge->stats.resonance_modulations++;

    /* Update average */
    float n = (float)bridge->stats.resonance_modulations;
    bridge->stats.avg_resonance_gain = (bridge->stats.avg_resonance_gain * (n - 1) + gain) / (fabsf(n) > 1e-7f ? n : 1e-7f);

    /* Check deception suppression */
    if (bridge->config.enable_deception_suppression &&
        mental_state->deception_likelihood > bridge->config.deception_suppress_threshold) {

        bridge->tom_effects.deception_suppression_active = true;
        bridge->tom_effects.suppressed_agent_id = mental_state->agent_id;
        bridge->tom_effects.imitation_gate = 0.0f;
        bridge->stats.deception_suppressions++;

        nimcp_log(LOG_LEVEL_INFO, "Mirror-ToM: deception suppression active for agent %u",
                  mental_state->agent_id);
    } else {
        bridge->tom_effects.deception_suppression_active = false;
        bridge->tom_effects.imitation_gate = gain;
    }

    return 0;
}

float mirror_tom_compute_resonance_gain(mirror_tom_bridge_t bridge,
                                         uint32_t agent_id) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_compute_resonance_gain: bridge is NULL");
        return 1.0f;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_compute_r", 0.0f);


    int32_t slot = find_agent_slot(bridge, agent_id);
    if (slot < 0) return 1.0f;  /* Default for unknown agents */

    return compute_modulation_from_mental_state(&bridge->agents[slot].mental_state,
                                                 &bridge->config);
}

bool mirror_tom_should_suppress_imitation(mirror_tom_bridge_t bridge,
                                           uint32_t agent_id) {
    if (!bridge || !bridge->config.enable_deception_suppression) {
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_should_su", 0.0f);


    int32_t slot = find_agent_slot(bridge, agent_id);
    if (slot < 0) {
        return false;
    }

    return bridge->agents[slot].mental_state.deception_likelihood >
           bridge->config.deception_suppress_threshold;
}

int mirror_tom_get_observation_bias(mirror_tom_bridge_t bridge,
                                     uint32_t agent_id,
                                     float* out_bias,
                                     float* out_confidence) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_obser", 0.0f);


    int32_t slot = find_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    mirror_tom_agent_state_t* agent = &bridge->agents[slot];

    /* Bias based on intention prediction */
    if (out_bias) {
        *out_bias = agent->mental_state.intention_alignment;
    }
    if (out_confidence) {
        *out_confidence = agent->mental_state.intention_confidence;
    }

    return agent->mental_state.intention_confidence > 0.1f ? 0 : -1;
}

/* ============================================================================
 * Core API - SIMD-Optimized Computations
 * ============================================================================ */

float mirror_tom_simd_similarity(const float* action,
                                  const float* intention,
                                  uint32_t dim) {
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_simd_simi", 0.0f);

    return nimcp_vector_cosine_similarity(action, intention, dim);
}

int mirror_tom_batch_belief_similarity(const float** belief_states,
                                        uint32_t num_agents,
                                        uint32_t belief_dim,
                                        float* out_similarity) {
    if (!belief_states || !out_similarity || num_agents == 0) {
        if (!belief_states || !out_similarity) {
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_batch_belief_similarity: required parameter is NULL");
            return -1;
        }
        return -1;
    }

    /* Compute pairwise similarities */
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_batch_bel", 0.0f);


    for (uint32_t i = 0; i < num_agents; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && num_agents > 256) {
            mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                             (float)(i + 1) / (float)num_agents);
        }

        out_similarity[i * num_agents + i] = 1.0f;  /* Self-similarity */

        for (uint32_t j = i + 1; j < num_agents; j++) {
            if (!belief_states[i] || !belief_states[j]) {
                out_similarity[i * num_agents + j] = 0.0f;
                out_similarity[j * num_agents + i] = 0.0f;
                continue;
            }

            float sim = nimcp_vector_cosine_similarity(belief_states[i], belief_states[j],
                                                       belief_dim);
            out_similarity[i * num_agents + j] = sim;
            out_similarity[j * num_agents + i] = sim;
        }
    }

    return 0;
}

/* ============================================================================
 * Core API - Query Functions
 * ============================================================================ */

int mirror_tom_get_mirror_effects(mirror_tom_bridge_t bridge,
                                   mirror_to_tom_effects_t* out_effects) {
    if (!bridge || !out_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_get_mirror_effects: required parameter is NULL");
        return -1;
    }
    *out_effects = bridge->mirror_effects;
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_mirro", 0.0f);


    return 0;
}

int mirror_tom_get_tom_effects(mirror_tom_bridge_t bridge,
                                tom_to_mirror_effects_t* out_effects) {
    if (!bridge || !out_effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_get_tom_effects: required parameter is NULL");
        return -1;
    }
    *out_effects = bridge->tom_effects;
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_tom_e", 0.0f);


    return 0;
}

int mirror_tom_get_agent_state(mirror_tom_bridge_t bridge,
                                uint32_t agent_id,
                                mirror_tom_agent_state_t* out_state) {
    if (!bridge || !out_state) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_get_agent_state: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_agent", 0.0f);


    int32_t slot = find_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "unknown: validation failed");
        return -1;
    }

    *out_state = bridge->agents[slot];
    return 0;
}

int mirror_tom_get_stats(mirror_tom_bridge_t bridge,
                          mirror_tom_stats_t* out_stats) {
    if (!bridge || !out_stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_get_stats: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_get_stats", 0.0f);


    bridge->stats.last_update_us = nimcp_time_now_us();
    *out_stats = bridge->stats;
    return 0;
}

/* ============================================================================
 * Core API - Update and Maintenance
 * ============================================================================ */

int mirror_tom_update(mirror_tom_bridge_t bridge, uint64_t delta_time_us) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Decay belief confidence over time */
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_update", 0.0f);


    float decay_factor = expf(-(float)delta_time_us / (float)BELIEF_DECAY_TIME_CONSTANT_US *
                               bridge->config.belief_decay_rate);

    uint64_t now = nimcp_time_now_us();

    for (uint32_t i = 0; i < MIRROR_TOM_MAX_AGENTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MIRROR_TOM_MAX_AGENTS > 256) {
            mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                             (float)(i + 1) / (float)MIRROR_TOM_MAX_AGENTS);
        }

        if (!bridge->agents[i].is_active) continue;

        mirror_tom_agent_state_t* agent = &bridge->agents[i];

        /* Decay belief confidence */
        for (uint32_t d = 0; d < agent->belief_dim; d++) {
            /* Phase 8: Loop progress heartbeat */
            if ((d & 0xFF) == 0 && agent->belief_dim > 256) {
                mirror_tom_bridge_heartbeat("mirror_tom_b_loop",
                                 (float)(d + 1) / (float)agent->belief_dim);
            }

            agent->belief_state[d] *= decay_factor;
        }

        /* Reset intention confidence over time */
        agent->mental_state.intention_confidence *= decay_factor;

        /* Check for stale agents (no observation for 5 minutes) */
        uint64_t age = now - agent->last_seen_us;
        if (age > 300000000ULL) {  /* 5 minutes in microseconds */
            /* Reduce trust for unseen agents */
            agent->mental_state.trust_level *= 0.99f;
        }
    }

    bridge->stats.last_update_us = now;

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_tom_reset_agent(mirror_tom_bridge_t bridge, uint32_t agent_id) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_reset_age", 0.0f);


    int32_t slot = find_agent_slot(bridge, agent_id);
    if (slot < 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_reset_agent: validation failed");
        return -1;
    }

    bridge->agents[slot].is_active = false;
    bridge->agent_count--;
    bridge->stats.agents_tracked = bridge->agent_count;

    nimcp_log(LOG_LEVEL_DEBUG, "Mirror-ToM: reset agent %u", agent_id);
    return 0;
}

/* ============================================================================
 * Bio-Async Registration
 * ============================================================================ */

bool mirror_tom_register_bio_async(mirror_tom_bridge_t bridge) {
    if (!bridge || bridge->bio_async_registered) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_tom_register_bio_async: bridge is NULL");
        return false;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_register_bio_async", 0.0f);

    bridge->bio_async_registered = true;
    return true;
}

void mirror_tom_unregister_bio_async(mirror_tom_bridge_t bridge) {
    if (!bridge || !bridge->bio_async_registered) return;

    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_unregister_bio_async", 0.0f);

    bridge->bio_async_registered = false;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* mirror_tom_emotion_name(mirror_tom_emotion_t emotion) {
    switch (emotion) {
        case MIRROR_TOM_EMOTION_UNKNOWN:  return "unknown";
        case MIRROR_TOM_EMOTION_NEUTRAL:  return "neutral";
        case MIRROR_TOM_EMOTION_JOY:      return "joy";
        case MIRROR_TOM_EMOTION_SADNESS:  return "sadness";
        case MIRROR_TOM_EMOTION_ANGER:    return "anger";
        case MIRROR_TOM_EMOTION_FEAR:     return "fear";
        case MIRROR_TOM_EMOTION_SURPRISE: return "surprise";
        case MIRROR_TOM_EMOTION_DISGUST:  return "disgust";
        default:                          return "invalid";
    }
}

void mirror_tom_print_observation(const mirror_tom_observation_t* obs,
                                   const char* prefix) {
    if (!obs) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_print_obs", 0.0f);


    const char* pfx = prefix ? prefix : "";

    nimcp_log(LOG_LEVEL_DEBUG, "%sObservation: dim=%u, resonance=%.2f, goal_conf=%.2f",
              pfx, obs->action_dim, obs->resonance_strength, obs->goal_confidence);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  emotion=%s (%.2f), PE=%.2f",
              pfx, mirror_tom_emotion_name(obs->emotional_resonance),
              obs->emotional_intensity, obs->prediction_error);
}

void mirror_tom_print_mental_state(const mirror_tom_mental_state_t* state,
                                    const char* prefix) {
    if (!state) return;
    /* Phase 8: Heartbeat at operation start */
    mirror_tom_bridge_heartbeat("mirror_tom_b_mirror_tom_print_men", 0.0f);


    const char* pfx = prefix ? prefix : "";

    nimcp_log(LOG_LEVEL_DEBUG, "%sMental State (agent %u):", pfx, state->agent_id);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  trust=%.2f, deception=%.2f, alignment=%.2f",
              pfx, state->trust_level, state->deception_likelihood, state->intention_alignment);
    nimcp_log(LOG_LEVEL_DEBUG, "%s  social_dist=%.2f, competence=%.2f, false_belief=%s",
              pfx, state->social_distance, state->competence_estimate,
              state->is_false_belief_holder ? "yes" : "no");
    if (state->predicted_intention[0] != '\0') {
        nimcp_log(LOG_LEVEL_DEBUG, "%s  predicted: %s (conf=%.2f)",
                  pfx, state->predicted_intention, state->intention_confidence);
    }
}

/* ============================================================================
 * B22: Instance Health Agent Setter
 * ============================================================================ */

void mirror_tom_bridge_set_instance_health_agent(
    mirror_tom_bridge_t bridge,
    nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/* ============================================================================
 * B22: Training Hook Stubs
 * ============================================================================ */

int mirror_tom_bridge_training_begin(mirror_tom_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_tom_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_tom_bridge_heartbeat("mirror_tom_b_training_begin", 0.0f);
    return 0;
}

int mirror_tom_bridge_training_end(mirror_tom_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_tom_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_tom_bridge_heartbeat("mirror_tom_b_training_end", 1.0f);
    return 0;
}

int mirror_tom_bridge_training_step(mirror_tom_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_tom_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_tom_bridge_heartbeat_instance(bridge->health_agent, "mirror_tom_bridge_training_step", progress);
    return 0;
}
