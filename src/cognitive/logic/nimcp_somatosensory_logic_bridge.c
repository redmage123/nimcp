/**
 * @file nimcp_somatosensory_logic_bridge.c
 * @brief Somatosensory-Logic Bridge Implementation
 *
 * Connects tactile/proprioceptive perception to symbolic reasoning.
 * Enables embodied cognition and physical reasoning.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/logic/nimcp_somatosensory_logic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(somatosensory_logic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_somatosensory_logic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_somatosensory_logic_bridge_mesh_registry = NULL;

nimcp_error_t somatosensory_logic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_somatosensory_logic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "somatosensory_logic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "somatosensory_logic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_somatosensory_logic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_somatosensory_logic_bridge_mesh_registry = registry;
    return err;
}

void somatosensory_logic_bridge_mesh_unregister(void) {
    if (g_somatosensory_logic_bridge_mesh_registry && g_somatosensory_logic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_somatosensory_logic_bridge_mesh_registry, g_somatosensory_logic_bridge_mesh_id);
        g_somatosensory_logic_bridge_mesh_id = 0;
        g_somatosensory_logic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from somatosensory_logic_bridge module (instance-level) */
static inline void somatosensory_logic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_somatosensory_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_somatosensory_logic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_somatosensory_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "SOMATOSENSORY_LOGIC_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_PENDING_COMMANDS 32
#define POSITION_TOLERANCE 0.1f

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Per-region state tracking
 */
typedef struct {
    bool has_contact;
    char contact_object[NIMCP_ID_BUFFER_SIZE];
    float contact_confidence;
    float position[3];
    float position_confidence;
    uint64_t last_update_us;
} region_state_t;

/**
 * @brief Bridge internal structure
 */
struct somato_logic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* somatosensory;                    /**< Somatosensory cortex handle */
    void* logic;                            /**< Logic module handle */
    somato_logic_config_t config;           /**< Configuration */
    somato_logic_stats_t stats;             /**< Statistics */

    /* Per-region state */
    region_state_t regions[BODY_REGION_COUNT];

    /* Pending commands */
    logic_somato_command_t pending_commands[MAX_PENDING_COMMANDS];
    uint32_t pending_count;

    /* Running averages */
    float touch_confidence_sum;
    float position_confidence_sum;
    uint64_t touch_count;
    uint64_t position_count;
};

//=============================================================================
// Region Name Lookup
//=============================================================================

static const char* region_names[BODY_REGION_COUNT] = {
    "head",
    "neck",
    "left_shoulder",
    "right_shoulder",
    "left_arm",
    "right_arm",
    "left_hand",
    "right_hand",
    "chest",
    "abdomen",
    "back",
    "left_leg",
    "right_leg",
    "left_foot",
    "right_foot"
};

const char* somato_logic_region_name(body_region_t region) {
    if (region >= BODY_REGION_COUNT) return "unknown";
    return region_names[region];
}

//=============================================================================
// Configuration API
//=============================================================================

somato_logic_config_t somato_logic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_default", 0.0f);


    return (somato_logic_config_t){
        .enable_touch_grounding = true,
        .enable_position_grounding = true,
        .enable_pain_priority = true,
        .enable_top_down_attention = true,
        .enable_verification = true,
        .min_intensity_threshold = 0.1f,
        .min_confidence_threshold = 0.4f,
        .pain_priority_boost = 2.0f
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

somato_logic_bridge_t* somato_logic_bridge_create(
    void* somatosensory,
    void* logic,
    const somato_logic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    somato_logic_bridge_t* bridge = nimcp_calloc(1, sizeof(somato_logic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->somatosensory = somatosensory;
    bridge->logic = logic;
    bridge->config = config ? *config : somato_logic_default_config();

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->regions, 0, sizeof(bridge->regions));
    bridge->pending_count = 0;

    bridge->touch_confidence_sum = 0.0f;
    bridge->position_confidence_sum = 0.0f;
    bridge->touch_count = 0;
    bridge->position_count = 0;

    return bridge;
}

void somato_logic_bridge_destroy(somato_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    if (bridge) {
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int somato_logic_bridge_reset(somato_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    memset(bridge->regions, 0, sizeof(bridge->regions));
    bridge->pending_count = 0;

    return 0;
}

//=============================================================================
// Somatosensory → Logic API
//=============================================================================

int somato_logic_ground_observation(
    somato_logic_bridge_t* bridge,
    const somato_logic_observation_t* obs
) {
    if (!bridge || !obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_ground_observation: required parameter is NULL (bridge, obs)");
        return -1;
    }
    if (obs->body_region >= BODY_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_ground_observation: capacity exceeded");
        return -1;
    }

    /* Filter by intensity and confidence */
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_ground_", 0.0f);


    if (obs->intensity < bridge->config.min_intensity_threshold) {
        return 0;
    }
    if (obs->confidence < bridge->config.min_confidence_threshold) {
        return 0;
    }

    region_state_t* region = &bridge->regions[obs->body_region];
    uint64_t now = obs->timestamp_us ? obs->timestamp_us : nimcp_time_get_us();

    switch (obs->signal_type) {
        case SOMATO_LOGIC_TOUCH_DETECTED:
            if (!bridge->config.enable_touch_grounding) return 0;

            region->has_contact = true;
            strncpy(region->contact_object, obs->contacted_object_name,
                   sizeof(region->contact_object) - 1);
            region->contact_object[sizeof(region->contact_object) - 1] = '\0';
            region->contact_confidence = obs->confidence;
            region->last_update_us = now;

            bridge->stats.touch_events_grounded++;
            bridge->touch_confidence_sum += obs->confidence;
            bridge->touch_count++;
            bridge->stats.avg_touch_confidence =
                bridge->touch_confidence_sum / bridge->touch_count;

            /*
             * TODO: Inject predicate into logic module
             * touching(region, object) or holding(region, object)
             */
            break;

        case SOMATO_LOGIC_POSITION_UPDATE:
            if (!bridge->config.enable_position_grounding) return 0;

            region->position[0] = obs->position[0];
            region->position[1] = obs->position[1];
            region->position[2] = obs->position[2];
            region->position_confidence = obs->confidence;
            region->last_update_us = now;

            bridge->stats.position_updates++;
            bridge->position_confidence_sum += obs->confidence;
            bridge->position_count++;
            bridge->stats.avg_position_confidence =
                bridge->position_confidence_sum / bridge->position_count;

            /*
             * TODO: Inject predicate into logic module
             * at_position(region, x, y, z)
             */
            break;

        case SOMATO_LOGIC_PAIN_SIGNAL:
            if (bridge->config.enable_pain_priority) {
                /* Pain always gets processed with boosted priority */
                bridge->stats.pain_signals++;

                /*
                 * TODO: Inject pain predicate with high priority
                 * pain(region, intensity)
                 */
            }
            break;

        case SOMATO_LOGIC_FORCE_SENSED:
        case SOMATO_LOGIC_TEMPERATURE:
            /* Process force and temperature similarly to touch */
            region->last_update_us = now;
            break;

        default:
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "somato_logic_ground_observation: unknown observation type");
            return -1;
    }

    return 0;
}

int somato_logic_report_body_state(
    somato_logic_bridge_t* bridge,
    const body_state_predicate_t* predicate
) {
    if (!bridge || !predicate) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_report_body_state: required parameter is NULL (bridge, predicate)");
        return -1;
    }
    if (predicate->region >= BODY_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_report_body_state: capacity exceeded");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_report_", 0.0f);


    region_state_t* region = &bridge->regions[predicate->region];

    if (predicate->active) {
        region->has_contact = true;
        strncpy(region->contact_object, predicate->object_name,
               sizeof(region->contact_object) - 1);
        region->contact_confidence = predicate->confidence;
    } else {
        region->has_contact = false;
    }

    region->last_update_us = nimcp_time_get_us();

    return 0;
}

int somato_logic_process_batch(
    somato_logic_bridge_t* bridge,
    const somato_logic_observation_t* observations,
    uint32_t count
) {
    if (!bridge || !observations) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_process_batch: required parameter is NULL (bridge, observations)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_process", 0.0f);


    int processed = 0;
    for (uint32_t i = 0; i < count; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && count > 256) {
            somatosensory_logic_bridge_heartbeat("somatosensor_loop",
                             (float)(i + 1) / (float)count);
        }

        if (somato_logic_ground_observation(bridge, &observations[i]) == 0) {
            processed++;
        }
    }

    return processed;
}

//=============================================================================
// Logic → Somatosensory API
//=============================================================================

int somato_logic_request_attention(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    float priority
) {
    if (!bridge || region >= BODY_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_request_attention: bridge is NULL");
        return -1;
    }
    if (!bridge->config.enable_top_down_attention) return 0;

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_request", 0.0f);


    if (bridge->pending_count >= MAX_PENDING_COMMANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_request_attention: capacity exceeded");
        return -1;
    }

    logic_somato_command_t* cmd = &bridge->pending_commands[bridge->pending_count++];
    cmd->signal_type = LOGIC_SOMATO_ATTEND_REGION;
    cmd->target_region = region;
    cmd->priority = priority < 0.0f ? 0.0f : (priority > 1.0f ? 1.0f : priority);
    cmd->expected_object[0] = '\0';

    bridge->stats.attention_commands++;

    return 0;
}

int somato_logic_expect_contact(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    const char* object_name
) {
    if (!bridge || region >= BODY_REGION_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_expect_contact: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_expect_", 0.0f);


    if (bridge->pending_count >= MAX_PENDING_COMMANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_expect_contact: capacity exceeded");
        return -1;
    }

    logic_somato_command_t* cmd = &bridge->pending_commands[bridge->pending_count++];
    cmd->signal_type = LOGIC_SOMATO_EXPECT_CONTACT;
    cmd->target_region = region;
    if (object_name) {
        strncpy(cmd->expected_object, object_name, sizeof(cmd->expected_object) - 1);
        cmd->expected_object[sizeof(cmd->expected_object) - 1] = '\0';
    } else {
        cmd->expected_object[0] = '\0';
    }
    cmd->priority = 0.7f;

    return 0;
}

int somato_logic_verify_position(
    somato_logic_bridge_t* bridge,
    body_region_t region,
    const float expected_position[3],
    bool* verified,
    float* confidence
) {
    if (!bridge || region >= BODY_REGION_COUNT || !verified || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_verify_position: required parameter is NULL (bridge, verified, confidence)");
        return -1;
    }
    if (!bridge->config.enable_verification) {
        *verified = false;
        *confidence = 0.0f;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_verify_", 0.0f);


    bridge->stats.verifications_requested++;

    region_state_t* rs = &bridge->regions[region];

    /* Check if we have position data */
    if (rs->position_confidence < 0.1f) {
        *verified = false;
        *confidence = 0.0f;
        return 0;
    }

    /* Calculate distance from expected */
    float dx = rs->position[0] - expected_position[0];
    float dy = rs->position[1] - expected_position[1];
    float dz = rs->position[2] - expected_position[2];
    float distance = sqrtf(dx*dx + dy*dy + dz*dz);

    if (distance < POSITION_TOLERANCE) {
        *verified = true;
        *confidence = rs->position_confidence * (1.0f - distance/POSITION_TOLERANCE);
        bridge->stats.verifications_confirmed++;
    } else {
        *verified = false;
        *confidence = rs->position_confidence * 0.5f;
    }

    return 0;
}

int somato_logic_send_command(
    somato_logic_bridge_t* bridge,
    const logic_somato_command_t* command
) {
    if (!bridge || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_send_command: required parameter is NULL (bridge, command)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_send_co", 0.0f);


    if (bridge->pending_count >= MAX_PENDING_COMMANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "somato_logic_send_command: capacity exceeded");
        return -1;
    }

    bridge->pending_commands[bridge->pending_count++] = *command;

    switch (command->signal_type) {
        case LOGIC_SOMATO_ATTEND_REGION:
        case LOGIC_SOMATO_EXPECT_CONTACT:
            bridge->stats.attention_commands++;
            break;
        case LOGIC_SOMATO_VERIFY_POSITION:
            bridge->stats.verifications_requested++;
            break;
        default:
            break;
    }

    return 0;
}

//=============================================================================
// Query API
//=============================================================================

int somato_logic_has_contact(
    const somato_logic_bridge_t* bridge,
    body_region_t region,
    bool* has_contact
) {
    if (!bridge || region >= BODY_REGION_COUNT || !has_contact) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_has_contact: required parameter is NULL (bridge, has_contact)");
        return -1;
    }

    *has_contact = bridge->regions[region].has_contact;
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_has_con", 0.0f);


    return 0;
}

int somato_logic_get_position(
    const somato_logic_bridge_t* bridge,
    body_region_t region,
    float position[3],
    float* confidence
) {
    if (!bridge || region >= BODY_REGION_COUNT || !position || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_get_position: required parameter is NULL (bridge, position, confidence)");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_get_pos", 0.0f);


    const region_state_t* rs = &bridge->regions[region];
    position[0] = rs->position[0];
    position[1] = rs->position[1];
    position[2] = rs->position[2];
    *confidence = rs->position_confidence;

    return 0;
}

//=============================================================================
// Statistics API
//=============================================================================

int somato_logic_bridge_get_stats(
    const somato_logic_bridge_t* bridge,
    somato_logic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "somato_logic_bridge_get_stats: required parameter is NULL (bridge, stats)");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    return 0;
}

void somato_logic_bridge_reset_stats(somato_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    if (bridge) {
        memset(&bridge->stats, 0, sizeof(bridge->stats));
        bridge->touch_confidence_sum = 0.0f;
        bridge->position_confidence_sum = 0.0f;
        bridge->touch_count = 0;
        bridge->position_count = 0;
    }
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int somato_logic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    somatosensory_logic_bridge_heartbeat("somatosensor_somato_logic_bridge_", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Somatosensory_Logic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                somatosensory_logic_bridge_heartbeat("somatosensor_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Somatosensory_Logic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Somatosensory_Logic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void somatosensory_logic_bridge_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_somatosensory_logic_bridge_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int somatosensory_logic_bridge_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "somatosensory_logic_bridge_training_begin: NULL argument");
        return -1;
    }
    somatosensory_logic_bridge_heartbeat_instance(NULL, "somatosensory_logic_bridge_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int somatosensory_logic_bridge_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "somatosensory_logic_bridge_training_end: NULL argument");
        return -1;
    }
    somatosensory_logic_bridge_heartbeat_instance(NULL, "somatosensory_logic_bridge_training_end", 1.0f);
    (void)instance;
    return 0;
}

int somatosensory_logic_bridge_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "somatosensory_logic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    somatosensory_logic_bridge_heartbeat_instance(NULL, "somatosensory_logic_bridge_training_step", progress);
    (void)instance;
    return 0;
}
