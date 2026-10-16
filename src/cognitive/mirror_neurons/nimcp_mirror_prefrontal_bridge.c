/**
 * @file nimcp_mirror_prefrontal_bridge.c
 * @brief Mirror-Prefrontal Cortex Integration Bridge Implementation
 * @version 1.0.0
 * @date 2026-01-05
 *
 * Implementation of bidirectional integration between mirror neurons and
 * prefrontal cortex for executive control over imitation and goal-directed
 * action planning.
 */

#include "cognitive/mirror_neurons/nimcp_mirror_prefrontal_bridge.h"
#include "utils/bridge/nimcp_bridge_base.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "async/nimcp_bio_router.h"
#include "utils/exception/nimcp_exception_macros.h"
#include "security/nimcp_bbb_helpers.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(mirror_prefrontal_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_mirror_prefrontal_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_mirror_prefrontal_bridge_mesh_registry = NULL;

nimcp_error_t mirror_prefrontal_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_mirror_prefrontal_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "mirror_prefrontal_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "mirror_prefrontal_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_mirror_prefrontal_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_mirror_prefrontal_bridge_mesh_registry = registry;
    return err;
}

void mirror_prefrontal_bridge_mesh_unregister(void) {
    if (g_mirror_prefrontal_bridge_mesh_registry && g_mirror_prefrontal_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_mirror_prefrontal_bridge_mesh_registry, g_mirror_prefrontal_bridge_mesh_id);
        g_mirror_prefrontal_bridge_mesh_id = 0;
        g_mirror_prefrontal_bridge_mesh_registry = NULL;
    }
}


static inline void mirror_prefrontal_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_mirror_prefrontal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_mirror_prefrontal_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_mirror_prefrontal_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Internal bridge structure
 */
struct mirror_prefrontal_bridge_struct {
    /* Base bridge support */
    bridge_base_t base;

    /* Configuration */
    mirror_prefrontal_config_t config;

    /* Connected systems (opaque handles) */
    void* mirror_system;
    void* prefrontal_adapter;

    /* Current state */
    float current_inhibition;
    imitation_mode_t imitation_mode;
    social_context_type_t social_context;
    uint32_t active_goal_id;

    /* Working memory for action sequences */
    action_sequence_t* sequences;
    uint32_t sequence_count;
    uint32_t next_sequence_id;

    /* Mirror neuron effects on PFC */
    float mirroring_activity;
    float empathic_signal;
    float action_prediction_confidence;
    uint32_t inferred_goal_id;

    /* Bio-async state */
    bool bio_async_enabled;
    bio_module_context_t bio_context;

    /* Statistics */
    mirror_prefrontal_stats_t stats;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Instance-level health agent (B22) */
    nimcp_health_agent_t* health_agent;
};

/* Security integration */
BRIDGE_DEFINE_SECURITY_SETTERS_TYPE(mirror_prefrontal_bridge, struct mirror_prefrontal_bridge_struct)

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get inhibition level for social context
 */
static float get_context_inhibition(social_context_type_t context) {
    switch (context) {
        case SOCIAL_CONTEXT_NONE:      return 0.5f;
        case SOCIAL_CONTEXT_FORMAL:    return 0.9f;
        case SOCIAL_CONTEXT_CASUAL:    return 0.5f;
        case SOCIAL_CONTEXT_LEARNING:  return 0.2f;
        case SOCIAL_CONTEXT_PLAYFUL:   return 0.1f;
        case SOCIAL_CONTEXT_EMERGENCY: return 0.0f;
        default:                       return 0.5f;
    }
}

/**
 * @brief Evaluate imitation decision based on current state
 */
static void evaluate_imitation_decision(
    mirror_prefrontal_bridge_t bridge,
    const imitation_request_t* request,
    imitation_decision_t* decision
) {
    decision->allowed = false;
    decision->inhibition_applied = 0.0f;
    decision->mode = bridge->imitation_mode;
    decision->goal_id = bridge->active_goal_id;
    memset(decision->reason, 0, sizeof(decision->reason));

    /* Check imitation mode */
    if (bridge->imitation_mode == IMITATION_MODE_BLOCKED) {
        decision->inhibition_applied = 1.0f;
        snprintf(decision->reason, sizeof(decision->reason), "Imitation blocked by PFC");
        return;
    }

    if (bridge->imitation_mode == IMITATION_MODE_UNRESTRICTED) {
        decision->allowed = true;
        decision->inhibition_applied = 0.0f;
        snprintf(decision->reason, sizeof(decision->reason), "Unrestricted learning mode");
        return;
    }

    /* Apply current inhibition level */
    float effective_inhibition = bridge->current_inhibition;

    /* Modulate by social context if enabled */
    if (bridge->config.enable_social_modulation) {
        float context_inhibition = get_context_inhibition(bridge->social_context);
        effective_inhibition = (effective_inhibition + context_inhibition) / 2.0f;
    }

    /* Check goal relevance in selective mode */
    if (bridge->imitation_mode == IMITATION_MODE_SELECTIVE) {
        if (request->goal_relevance < bridge->config.goal_relevance_threshold) {
            decision->inhibition_applied = 1.0f;
            snprintf(decision->reason, sizeof(decision->reason),
                     "Low goal relevance (%.2f < %.2f)",
                     request->goal_relevance, bridge->config.goal_relevance_threshold);
            return;
        }
        /* Reduce inhibition for goal-relevant actions */
        effective_inhibition *= (1.0f - request->goal_relevance);
    }

    /* Final decision based on resonance vs inhibition */
    decision->inhibition_applied = effective_inhibition;

    if (request->resonance_strength > effective_inhibition) {
        decision->allowed = true;
        snprintf(decision->reason, sizeof(decision->reason),
                 "Resonance (%.2f) exceeds inhibition (%.2f)",
                 request->resonance_strength, effective_inhibition);
    } else {
        snprintf(decision->reason, sizeof(decision->reason),
                 "Inhibition (%.2f) exceeds resonance (%.2f)",
                 effective_inhibition, request->resonance_strength);
    }
}

/**
 * @brief Find sequence slot in working memory
 */
static int find_sequence_slot(mirror_prefrontal_bridge_t bridge, uint32_t sequence_id) {
    for (uint32_t i = 0; i < bridge->sequence_count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->sequence_count > 256) {
            mirror_prefrontal_bridge_heartbeat("mirror_prefr_loop",
                             (float)(i + 1) / (float)bridge->sequence_count);
        }

        if (bridge->sequences[i].sequence_id == sequence_id) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Bio-async message handler
 *
 * Note: This is a stub implementation. The actual bio-async message handling
 * would be done via bio_router_register_handler with specific message types.
 * For now, we track messages as statistics and provide logging.
 */
static void handle_bio_message(
    bio_module_context_t ctx,
    bio_message_type_t msg_type,
    const void* payload,
    uint32_t payload_size,
    void* user_data
) {
    (void)ctx; /* Unused in stub */
    mirror_prefrontal_bridge_t bridge = (mirror_prefrontal_bridge_t)user_data;
    if (!bridge) return;

    nimcp_mutex_lock(bridge->base.mutex);
    bridge->stats.messages_received++;

    switch (msg_type) {
        case BIO_MSG_MIRROR_OBSERVATION_START:
            /* Agent detected - prepare for observation */
            if (bridge->config.enable_verbose_logging) {
                NIMCP_LOGGING_DEBUG("Mirror-PFC: Observation started");
            }
            break;

        case BIO_MSG_MIRROR_OBSERVATION_END:
            /* Observation complete - evaluate for storage */
            if (bridge->config.enable_verbose_logging) {
                NIMCP_LOGGING_DEBUG("Mirror-PFC: Observation ended");
            }
            break;

        case BIO_MSG_MIRROR_RESONANCE_TRIGGERED:
            /* Motor resonance above threshold - evaluate inhibition */
            bridge->mirroring_activity = 1.0f; /* Mark as active */
            if (bridge->config.enable_verbose_logging) {
                NIMCP_LOGGING_DEBUG("Mirror-PFC: Resonance triggered");
            }
            break;

        case BIO_MSG_MIRROR_GOAL_INFERRED:
            /* Hierarchy system inferred goal */
            if (payload && payload_size >= sizeof(uint32_t)) {
                bridge->inferred_goal_id = *((const uint32_t*)payload);
                bridge->stats.goals_inferred++;
            }
            break;

        case BIO_MSG_MIRROR_SOCIAL_CONTEXT_CHANGE:
            /* Social context updated from external source */
            if (payload && payload_size >= sizeof(social_context_type_t)) {
                bridge->social_context = *((const social_context_type_t*)payload);
                bridge->stats.context_switches++;
            }
            break;

        case BIO_MSG_VISUAL_AGENT_DETECTED:
            /* Visual cortex detected agent - activate observation mode */
            if (bridge->config.enable_verbose_logging) {
                NIMCP_LOGGING_DEBUG("Mirror-PFC: Agent detected via visual cortex");
            }
            break;

        case BIO_MSG_MOTOR_ACTION_EXECUTED:
            /* Motor action executed - for STDP learning */
            if (bridge->config.enable_verbose_logging) {
                NIMCP_LOGGING_DEBUG("Mirror-PFC: Motor action executed notification");
            }
            break;

        default:
            /* Unhandled message type */
            break;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
}

/*=============================================================================
 * LIFECYCLE FUNCTIONS
 *===========================================================================*/

int mirror_prefrontal_default_config(mirror_prefrontal_config_t* config) {
    if (!config) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "config is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_de", 0.0f);


    memset(config, 0, sizeof(mirror_prefrontal_config_t));

    /* Inhibitory control parameters */
    config->inhibition_threshold = MIRROR_PFC_INHIBITION_THRESHOLD_DEFAULT;
    config->inhibition_decay_rate = 0.01f;
    config->enable_automatic_inhibition = true;

    /* Working memory integration */
    config->wm_sequence_capacity = MIRROR_PFC_WM_SEQUENCE_CAPACITY_DEFAULT;
    config->wm_max_actions_per_seq = MIRROR_PFC_WM_MAX_ACTIONS_PER_SEQUENCE;
    config->enable_wm_integration = true;
    config->wm_priority_boost = 0.2f;

    /* Goal-directed control */
    config->goal_relevance_threshold = MIRROR_PFC_GOAL_RELEVANCE_THRESHOLD;
    config->enable_goal_filtering = true;
    config->enable_hierarchical_goals = true;

    /* Social context */
    config->social_sensitivity = MIRROR_PFC_SOCIAL_SENSITIVITY_DEFAULT;
    config->enable_social_modulation = true;
    config->default_context = SOCIAL_CONTEXT_CASUAL;

    /* Bio-async communication */
    config->enable_bio_async = true;

    /* Debugging */
    config->enable_verbose_logging = false;

    return 0;
}

mirror_prefrontal_bridge_t mirror_prefrontal_bridge_create(
    const mirror_prefrontal_config_t* config,
    void* mirror,
    void* prefrontal
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_create", 0.0f);


    mirror_prefrontal_bridge_t bridge = nimcp_calloc(1, sizeof(struct mirror_prefrontal_bridge_struct));
    if (!bridge) {
        NIMCP_LOGGING_ERROR("Mirror-PFC bridge: Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;
    }

    /* Initialize mutex */
    if (bridge_base_init(&bridge->base, 0, "mirror_prefrontal") != 0) { nimcp_free(bridge); return NULL; }
    if (!bridge->base.mutex) {
        NIMCP_LOGGING_ERROR("Mirror-PFC bridge: Failed to create mutex");
        nimcp_free(bridge);
        bridge = NULL;
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_prefrontal_bridge_create: bridge->base is NULL");
        return NULL;
    }

    /* Copy configuration or use defaults */
    if (config) {
        bridge->config = *config;
    } else {
        mirror_prefrontal_default_config(&bridge->config);
    }

    /* Store system references */
    bridge->mirror_system = mirror;
    bridge->prefrontal_adapter = prefrontal;

    /* Initialize state */
    bridge->current_inhibition = bridge->config.inhibition_threshold;
    bridge->imitation_mode = IMITATION_MODE_CONTEXTUAL;
    bridge->social_context = bridge->config.default_context;
    bridge->active_goal_id = 0;

    /* Initialize mirror effects */
    bridge->mirroring_activity = 0.0f;
    bridge->empathic_signal = 0.0f;
    bridge->action_prediction_confidence = 0.0f;
    bridge->inferred_goal_id = 0;

    /* Allocate working memory for sequences */
    if (bridge->config.enable_wm_integration && bridge->config.wm_sequence_capacity > 0) {
        bridge->sequences = nimcp_calloc(
            bridge->config.wm_sequence_capacity,
            sizeof(action_sequence_t)
        );
        if (!bridge->sequences) {
            NIMCP_LOGGING_ERROR("Mirror-PFC bridge: Failed to allocate sequence memory");
            bridge_base_cleanup(&bridge->base);
            nimcp_free(bridge);
            bridge = NULL;
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "mirror_prefrontal_bridge_create: bridge->sequences is NULL");
            return NULL;
        }
    }
    bridge->sequence_count = 0;
    bridge->next_sequence_id = 1;

    /* Initialize bio-async state */
    bridge->bio_async_enabled = false;
    bridge->bio_context = NULL;

    /* Initialize statistics */
    memset(&bridge->stats, 0, sizeof(mirror_prefrontal_stats_t));

    NIMCP_LOGGING_INFO("Mirror-PFC bridge: Created successfully");
    return bridge;
}

void mirror_prefrontal_bridge_destroy(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) return;

    /* Disconnect bio-async if connected */
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_destroy", 0.0f);


    if (bridge->bio_async_enabled) {
        mirror_prefrontal_disconnect_bio_async(bridge);
    }

    /* Free working memory sequences */
    if (bridge->sequences) {
        nimcp_free(bridge->sequences);
    }

    /* Destroy mutex */
    if (bridge->base.mutex) {
        bridge_base_cleanup(&bridge->base);
    }

    nimcp_free(bridge);
    bridge = NULL;
    NIMCP_LOGGING_INFO("Mirror-PFC bridge: Destroyed");
}

int mirror_prefrontal_bridge_reset(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_reset", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Reset state */
    bridge->current_inhibition = bridge->config.inhibition_threshold;
    bridge->imitation_mode = IMITATION_MODE_CONTEXTUAL;
    bridge->social_context = bridge->config.default_context;
    bridge->active_goal_id = 0;

    /* Reset mirror effects */
    bridge->mirroring_activity = 0.0f;
    bridge->empathic_signal = 0.0f;
    bridge->action_prediction_confidence = 0.0f;
    bridge->inferred_goal_id = 0;

    /* Clear working memory */
    if (bridge->sequences) {
        memset(bridge->sequences, 0,
               bridge->config.wm_sequence_capacity * sizeof(action_sequence_t));
    }
    bridge->sequence_count = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC bridge: Reset complete");
    return 0;
}

/*=============================================================================
 * INHIBITORY CONTROL API
 *===========================================================================*/

int mirror_prefrontal_request_imitation(
    mirror_prefrontal_bridge_t bridge,
    const imitation_request_t* request,
    imitation_decision_t* decision
) {
    if (!bridge || !request || !decision) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_request_imitation: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    evaluate_imitation_decision(bridge, request, decision);

    /* Update statistics */
    if (decision->allowed) {
        bridge->stats.imitations_allowed++;
        if (bridge->active_goal_id != 0 &&
            request->goal_relevance >= bridge->config.goal_relevance_threshold) {
            bridge->stats.goal_guided_imitations++;
        }
    } else {
        bridge->stats.imitations_blocked++;
        if (bridge->config.enable_social_modulation &&
            get_context_inhibition(bridge->social_context) > 0.5f) {
            bridge->stats.context_inhibitions++;
        }
    }

    /* Update running average of inhibition */
    uint64_t total = bridge->stats.imitations_allowed + bridge->stats.imitations_blocked;
    if (total > 0) {
        bridge->stats.avg_inhibition_level =
            (bridge->stats.avg_inhibition_level * (total - 1) + decision->inhibition_applied) / total;
    }

    /* Send bio-async notification if connected */
    if (bridge->bio_async_enabled && bridge->bio_context) {
        bio_message_header_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = decision->allowed ? BIO_MSG_MIRROR_PREFRONTAL_RELEASE : BIO_MSG_MIRROR_PREFRONTAL_INHIBIT;
        msg.source_module = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
        msg.payload_size = sizeof(imitation_decision_t);
        /* Note: In full implementation, would use bio_router_send with proper payload handling */
        (void)msg; /* Stub - bio-async integration would be completed in production */
        bridge->stats.messages_sent++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

int mirror_prefrontal_set_inhibition(
    mirror_prefrontal_bridge_t bridge,
    float level
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->current_inhibition = level < 0.0f ? 0.0f : (level > 1.0f ? 1.0f : level);
    nimcp_mutex_unlock(bridge->base.mutex);

    return 0;
}

float mirror_prefrontal_get_inhibition(const mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_inhibition: bridge is NULL");
        return -1.0f;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    return bridge->current_inhibition;
}

int mirror_prefrontal_set_imitation_mode(
    mirror_prefrontal_bridge_t bridge,
    imitation_mode_t mode
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->imitation_mode = mode;

    if (mode == IMITATION_MODE_UNRESTRICTED) {
        bridge->stats.inhibition_releases++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

imitation_mode_t mirror_prefrontal_get_imitation_mode(
    const mirror_prefrontal_bridge_t bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_imitation_mode: bridge is NULL");
        return IMITATION_MODE_BLOCKED;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    return bridge->imitation_mode;
}

/*=============================================================================
 * SOCIAL CONTEXT API
 *===========================================================================*/

int mirror_prefrontal_set_social_context(
    mirror_prefrontal_bridge_t bridge,
    social_context_type_t context
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->social_context != context) {
        bridge->social_context = context;
        bridge->stats.context_switches++;

        /* Adjust inhibition if automatic modulation enabled */
        if (bridge->config.enable_automatic_inhibition) {
            bridge->current_inhibition = get_context_inhibition(context);
        }

        /* Send bio-async notification */
        if (bridge->bio_async_enabled && bridge->bio_context) {
            bio_message_header_t msg;
            memset(&msg, 0, sizeof(msg));
            msg.type = BIO_MSG_MIRROR_SOCIAL_CONTEXT_CHANGE;
            msg.source_module = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
            msg.payload_size = sizeof(social_context_type_t);
            /* Note: In full implementation, would use bio_router_send */
            (void)msg; /* Stub */
            bridge->stats.messages_sent++;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

social_context_type_t mirror_prefrontal_get_social_context(
    const mirror_prefrontal_bridge_t bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_social_context: bridge is NULL");
        return SOCIAL_CONTEXT_NONE;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    return bridge->social_context;
}

/*=============================================================================
 * WORKING MEMORY INTEGRATION API
 *===========================================================================*/

uint32_t mirror_prefrontal_store_sequence(
    mirror_prefrontal_bridge_t bridge,
    const action_sequence_t* sequence
) {
    if (!bridge || !sequence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_store_sequence: required parameter is NULL");
        return 0;
    }
    BRIDGE_BBB_VALIDATE(bridge, sequence, sizeof(*sequence));
    if (!bridge->config.enable_wm_integration || !bridge->sequences) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_st", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Check capacity */
    if (bridge->sequence_count >= bridge->config.wm_sequence_capacity) {
        NIMCP_LOGGING_WARN("Mirror-PFC: Working memory at capacity");
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Find empty slot */
    int slot = -1;
    for (uint32_t i = 0; i < bridge->config.wm_sequence_capacity; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && bridge->config.wm_sequence_capacity > 256) {
            mirror_prefrontal_bridge_heartbeat("mirror_prefr_loop",
                             (float)(i + 1) / (float)bridge->config.wm_sequence_capacity);
        }

        if (bridge->sequences[i].sequence_id == 0) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        return 0;
    }

    /* Store sequence */
    bridge->sequences[slot] = *sequence;
    bridge->sequences[slot].sequence_id = bridge->next_sequence_id++;
    bridge->sequence_count++;

    /* Update statistics */
    bridge->stats.sequences_stored++;
    uint64_t total = bridge->stats.sequences_stored;
    bridge->stats.avg_sequence_length =
        (bridge->stats.avg_sequence_length * (total - 1) + sequence->action_count) / (total > 0 ? total : 1);

    /* Send bio-async notification */
    if (bridge->bio_async_enabled && bridge->bio_context) {
        bio_message_header_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = BIO_MSG_MIRROR_WORKING_MEMORY_STORE;
        msg.source_module = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
        msg.payload_size = sizeof(uint32_t);
        /* Note: In full implementation, would use bio_router_send */
        (void)msg; /* Stub */
        bridge->stats.messages_sent++;
    }

    uint32_t id = bridge->sequences[slot].sequence_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC: Stored sequence %u with %u actions",
                        id, sequence->action_count);
    return id;
}

int mirror_prefrontal_recall_sequence(
    mirror_prefrontal_bridge_t bridge,
    uint32_t sequence_id,
    action_sequence_t* sequence
) {
    if (!bridge || !sequence || sequence_id == 0) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_recall_sequence: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_wm_integration || !bridge->sequences) return -1;

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int slot = find_sequence_slot(bridge, sequence_id);
    if (slot < 0) {
        nimcp_mutex_unlock(bridge->base.mutex);
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_recall_sequence: validation failed");
        return -1;
    }

    *sequence = bridge->sequences[slot];
    bridge->stats.sequences_recalled++;

    /* Send bio-async notification */
    if (bridge->bio_async_enabled && bridge->bio_context) {
        bio_message_header_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = BIO_MSG_MIRROR_WORKING_MEMORY_RECALL;
        msg.source_module = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
        msg.payload_size = sizeof(uint32_t);
        /* Note: In full implementation, would use bio_router_send */
        (void)msg; /* Stub */
        bridge->stats.messages_sent++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return 0;
}

uint32_t mirror_prefrontal_get_sequence_count(
    const mirror_prefrontal_bridge_t bridge
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_sequence_count: bridge is NULL");
        return 0;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    return bridge->sequence_count;
}

int mirror_prefrontal_clear_sequences(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->sequences) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_cl", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    int cleared = (int)bridge->sequence_count;
    memset(bridge->sequences, 0,
           bridge->config.wm_sequence_capacity * sizeof(action_sequence_t));
    bridge->sequence_count = 0;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC: Cleared %d sequences from working memory", cleared);
    return cleared;
}

/*=============================================================================
 * GOAL-DIRECTED CONTROL API
 *===========================================================================*/

int mirror_prefrontal_set_active_goal(
    mirror_prefrontal_bridge_t bridge,
    uint32_t goal_id
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_se", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    bridge->active_goal_id = goal_id;
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC: Active goal set to %u", goal_id);
    return 0;
}

float mirror_prefrontal_get_goal_relevance(
    mirror_prefrontal_bridge_t bridge,
    uint32_t action_id
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_goal_relevance: bridge is NULL");
        return -1.0f;
    }

    /* If no active goal, all actions have neutral relevance */
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    if (bridge->active_goal_id == 0) {
        return 0.5f;
    }

    /* In a full implementation, this would query the prefrontal adapter
     * to determine action-goal relevance. For now, return moderate relevance. */
    (void)action_id;
    return 0.5f;
}

int mirror_prefrontal_notify_goal_inference(
    mirror_prefrontal_bridge_t bridge,
    uint32_t inferred_goal_id,
    float confidence
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_no", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    bridge->inferred_goal_id = inferred_goal_id;
    bridge->action_prediction_confidence = confidence;
    bridge->stats.goals_inferred++;

    /* Update running average of goal relevance */
    uint64_t total = bridge->stats.goals_inferred;
    bridge->stats.avg_goal_relevance =
        (bridge->stats.avg_goal_relevance * (total - 1) + confidence) / (total > 0 ? total : 1);

    /* Send bio-async notification */
    if (bridge->bio_async_enabled && bridge->bio_context) {
        bio_message_header_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = BIO_MSG_MIRROR_GOAL_INFERRED;
        msg.source_module = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
        msg.payload_size = sizeof(uint32_t);
        /* Note: In full implementation, would use bio_router_send */
        (void)msg; /* Stub */
        (void)inferred_goal_id; /* Used in production */
        bridge->stats.messages_sent++;
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC: Goal %u inferred with confidence %.2f",
                        inferred_goal_id, confidence);
    return 0;
}

/*=============================================================================
 * UPDATE AND EFFECTS API
 *===========================================================================*/

int mirror_prefrontal_update(
    mirror_prefrontal_bridge_t bridge,
    float dt_ms
) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_up", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Decay inhibition towards context-appropriate level */
    if (bridge->config.inhibition_decay_rate > 0.0f) {
        float target_inhibition = get_context_inhibition(bridge->social_context);
        float decay = bridge->config.inhibition_decay_rate * (dt_ms / 1000.0f);

        if (bridge->current_inhibition > target_inhibition) {
            bridge->current_inhibition -= decay;
            if (bridge->current_inhibition < target_inhibition) {
                bridge->current_inhibition = target_inhibition;
            }
        } else if (bridge->current_inhibition < target_inhibition) {
            bridge->current_inhibition += decay;
            if (bridge->current_inhibition > target_inhibition) {
                bridge->current_inhibition = target_inhibition;
            }
        }
    }

    /* Decay mirroring activity */
    if (bridge->mirroring_activity > 0.0f) {
        bridge->mirroring_activity -= 0.01f * (dt_ms / 1000.0f);
        if (bridge->mirroring_activity < 0.0f) {
            bridge->mirroring_activity = 0.0f;
        }
    }

    /* Decay empathic signal */
    if (bridge->empathic_signal > 0.0f) {
        bridge->empathic_signal -= 0.005f * (dt_ms / 1000.0f);
        if (bridge->empathic_signal < 0.0f) {
            bridge->empathic_signal = 0.0f;
        }
    }

    nimcp_mutex_unlock(bridge->base.mutex);

    /* Notify coordinator of update cycle completion */
    bridge_base_notify_coordinator_tick(&bridge->base, 0);
    return 0;
}

int mirror_prefrontal_get_effects(
    const mirror_prefrontal_bridge_t bridge,
    mirror_prefrontal_effects_t* effects
) {
    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_effects: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    nimcp_mutex_lock(((mirror_prefrontal_bridge_t)bridge)->mutex);

    /* PFC -> Mirror effects */
    effects->current_inhibition = bridge->current_inhibition;
    effects->imitation_mode = bridge->imitation_mode;
    effects->social_context = bridge->social_context;
    effects->active_goal_id = bridge->active_goal_id;

    /* Mirror -> PFC effects */
    effects->mirroring_activity = bridge->mirroring_activity;
    effects->empathic_signal = bridge->empathic_signal;
    effects->action_prediction_confidence = bridge->action_prediction_confidence;
    effects->inferred_goal_id = bridge->inferred_goal_id;

    /* Working memory state */
    effects->sequences_stored = bridge->sequence_count;
    effects->wm_at_capacity =
        (bridge->sequence_count >= bridge->config.wm_sequence_capacity);

    /* Integration state */
    effects->bridge_active = true;
    effects->last_update_time = 0; /* Would need actual timestamp */

    nimcp_mutex_unlock(((mirror_prefrontal_bridge_t)bridge)->mutex);
    return 0;
}

/*=============================================================================
 * BIO-ASYNC API
 *===========================================================================*/

int mirror_prefrontal_connect_bio_async(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (bridge->bio_async_enabled) return 0; /* Already connected */

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_co", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    /* Register with bio-async router */
    bio_module_info_t info;
    memset(&info, 0, sizeof(info));
    info.module_id = BIO_MODULE_MIRROR_PREFRONTAL_BRIDGE_ID;
    info.module_name = "Mirror-Prefrontal Bridge";
    info.user_data = bridge;

    bridge->bio_context = bio_router_register_module(&info);

    if (bridge->bio_context) {
        bridge->bio_async_enabled = true;
        NIMCP_LOGGING_INFO("Mirror-PFC bridge: Bio-async connected");
    } else {
        NIMCP_LOGGING_WARN("Mirror-PFC bridge: Bio-async connection failed");
    }

    nimcp_mutex_unlock(bridge->base.mutex);
    return bridge->bio_async_enabled ? 0 : -1;
}

int mirror_prefrontal_disconnect_bio_async(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }
    if (!bridge->bio_async_enabled) return 0;

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_di", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);

    if (bridge->bio_context) {
        bio_router_unregister_module(bridge->bio_context);
        bridge->bio_context = NULL;
    }
    bridge->bio_async_enabled = false;

    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_INFO("Mirror-PFC bridge: Bio-async disconnected");
    return 0;
}

bool mirror_prefrontal_is_bio_async_connected(
    const mirror_prefrontal_bridge_t bridge
) {
    if (!bridge) {
        return false;
    }
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_is", 0.0f);


    return bridge->bio_async_enabled;
}

uint32_t mirror_prefrontal_process_messages(
    mirror_prefrontal_bridge_t bridge,
    uint32_t max_messages
) {
    if (!bridge || !bridge->bio_async_enabled || !bridge->bio_context) {
        if (!bridge) NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_process_messages: bridge is NULL");
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_pr", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    uint32_t processed = 0;
    uint32_t limit = (max_messages == 0) ? 32 : max_messages;

    /* Update mirror-prefrontal integration state based on current activity.
     * Process any pending action predictions and inhibition updates. */
    if (bridge->mirroring_activity > 0.0f && processed < limit) {
        /* Propagate mirroring activity to prefrontal system */
        float inhibition = bridge->current_inhibition;

        /* If inhibition is high, suppress mirroring signal */
        float effective_signal = bridge->mirroring_activity * (1.0f - inhibition);

        /* Update action prediction confidence based on effective signal */
        bridge->action_prediction_confidence =
            0.9f * bridge->action_prediction_confidence +
            0.1f * effective_signal;

        bridge->stats.messages_received++;
        processed++;
    }

    /* Process empathic signal if present */
    if (bridge->empathic_signal > 0.1f && processed < limit) {
        /* Empathic signals modulate inhibition level */
        float empathy_modulation = bridge->empathic_signal * 0.1f;
        bridge->current_inhibition =
            bridge->current_inhibition * (1.0f - empathy_modulation);
        if (bridge->current_inhibition < 0.0f)
            bridge->current_inhibition = 0.0f;

        bridge->stats.messages_received++;
        processed++;
    }

    nimcp_mutex_unlock(bridge->mutex);

    return processed;
}

/*=============================================================================
 * STATISTICS API
 *===========================================================================*/

int mirror_prefrontal_get_stats(
    const mirror_prefrontal_bridge_t bridge,
    mirror_prefrontal_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "mirror_prefrontal_get_stats: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_ge", 0.0f);


    nimcp_mutex_lock(((mirror_prefrontal_bridge_t)bridge)->mutex);
    *stats = bridge->stats;
    nimcp_mutex_unlock(((mirror_prefrontal_bridge_t)bridge)->mutex);

    return 0;
}

int mirror_prefrontal_reset_stats(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_re", 0.0f);


    nimcp_mutex_lock(bridge->base.mutex);
    memset(&bridge->stats, 0, sizeof(mirror_prefrontal_stats_t));
    nimcp_mutex_unlock(bridge->base.mutex);

    NIMCP_LOGGING_DEBUG("Mirror-PFC bridge: Statistics reset");
    return 0;
}

/*=============================================================================
 * UTILITY FUNCTIONS
 *===========================================================================*/

const char* mirror_prefrontal_social_context_name(social_context_type_t context) {
    switch (context) {
        case SOCIAL_CONTEXT_NONE:      return "None";
        case SOCIAL_CONTEXT_FORMAL:    return "Formal";
        case SOCIAL_CONTEXT_CASUAL:    return "Casual";
        case SOCIAL_CONTEXT_LEARNING:  return "Learning";
        case SOCIAL_CONTEXT_PLAYFUL:   return "Playful";
        case SOCIAL_CONTEXT_EMERGENCY: return "Emergency";
        default:                       return "Unknown";
    }
}

const char* mirror_prefrontal_imitation_mode_name(imitation_mode_t mode) {
    switch (mode) {
        case IMITATION_MODE_BLOCKED:      return "Blocked";
        case IMITATION_MODE_SELECTIVE:    return "Selective";
        case IMITATION_MODE_CONTEXTUAL:   return "Contextual";
        case IMITATION_MODE_UNRESTRICTED: return "Unrestricted";
        default:                          return "Unknown";
    }
}

float mirror_prefrontal_default_inhibition_for_context(
    social_context_type_t context
) {
    /* Phase 8: Heartbeat at operation start */
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_mirror_prefrontal_de", 0.0f);


    return get_context_inhibition(context);
}

/*=============================================================================
 * B22: Instance Health Agent Setter
 *===========================================================================*/

void mirror_prefrontal_bridge_set_instance_health_agent(
    mirror_prefrontal_bridge_t bridge,
    nimcp_health_agent_t* agent)
{
    if (bridge) {
        bridge->health_agent = agent;
    }
}

/*=============================================================================
 * B22: Training Hook Stubs
 *===========================================================================*/

int mirror_prefrontal_bridge_training_begin(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_prefrontal_bridge_training_begin: NULL argument");
        return -1;
    }
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_training_begin", 0.0f);
    return 0;
}

int mirror_prefrontal_bridge_training_end(mirror_prefrontal_bridge_t bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_prefrontal_bridge_training_end: NULL argument");
        return -1;
    }
    mirror_prefrontal_bridge_heartbeat("mirror_prefr_training_end", 1.0f);
    return 0;
}

int mirror_prefrontal_bridge_training_step(mirror_prefrontal_bridge_t bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "mirror_prefrontal_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    mirror_prefrontal_bridge_heartbeat_instance(bridge->health_agent, "mirror_prefrontal_bridge_training_step", progress);
    return 0;
}
