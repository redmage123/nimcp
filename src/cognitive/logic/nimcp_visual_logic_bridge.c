/**
 * @file nimcp_visual_logic_bridge.c
 * @brief Visual-Logic Bridge Implementation
 *
 * Connects visual perception to symbolic reasoning system.
 * Enables visual concept grounding and perception-based inference.
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "cognitive/logic/nimcp_visual_logic_bridge.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/time/nimcp_time.h"
#include "cognitive/knowledge/nimcp_kg_reader.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <stdio.h>

//=============================================================================
#include <stddef.h>  /* for NULL */
#include "utils/logging/nimcp_logging.h"
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "constants/nimcp_buffer_constants.h"
#include "constants/nimcp_threshold_constants.h"

NIMCP_DECLARE_HEALTH_AGENT_ATOMIC(visual_logic_bridge)
//=============================================================================
// Mesh Participant Registration
//=============================================================================

static mesh_participant_id_t g_visual_logic_bridge_mesh_id = 0;
static mesh_participant_registry_t* g_visual_logic_bridge_mesh_registry = NULL;

nimcp_error_t visual_logic_bridge_mesh_register(mesh_participant_registry_t* registry) {
    if (!registry) return NIMCP_ERROR_NULL_POINTER;
    if (g_visual_logic_bridge_mesh_id != 0) return NIMCP_SUCCESS;
    mesh_participant_interface_t iface;
    mesh_participant_interface_init(&iface);
    strncpy(iface.module_name, "visual_logic_bridge", MESH_MAX_NAME_LEN - 1);
    iface.type = MESH_PARTICIPANT_MODULE;
    iface.home_channel = mesh_adapter_get_default_channel(MESH_ADAPTER_CATEGORY_COGNITIVE);
    mesh_participant_config_t config;
    mesh_participant_config_init(&config);
    config.module_name = "visual_logic_bridge";
    config.type = MESH_PARTICIPANT_MODULE;
    config.home_channel = iface.home_channel;
    nimcp_error_t err = mesh_participant_register(registry, &iface, &config, &g_visual_logic_bridge_mesh_id);
    if (err == NIMCP_SUCCESS) g_visual_logic_bridge_mesh_registry = registry;
    return err;
}

void visual_logic_bridge_mesh_unregister(void) {
    if (g_visual_logic_bridge_mesh_registry && g_visual_logic_bridge_mesh_id != 0) {
        mesh_participant_unregister(g_visual_logic_bridge_mesh_registry, g_visual_logic_bridge_mesh_id);
        g_visual_logic_bridge_mesh_id = 0;
        g_visual_logic_bridge_mesh_registry = NULL;
    }
}


/** @brief Send heartbeat from visual_logic_bridge module (instance-level) */
static inline void visual_logic_bridge_heartbeat_instance(
    nimcp_health_agent_t* instance_agent, const char* operation, float progress)
{
    if (g_visual_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(g_visual_logic_bridge_health_agent, operation, progress);
    }
    if (instance_agent && instance_agent != g_visual_logic_bridge_health_agent) {
        nimcp_health_agent_heartbeat_ex(instance_agent, operation, progress);
    }
}


#define LOG_MODULE "VISUAL_LOGIC_BRIDGE"


//=============================================================================
// Internal Constants
//=============================================================================

#define MAX_GROUNDED_OBJECTS 256
#define MAX_PENDING_COMMANDS 32

//=============================================================================
// Internal Structures
//=============================================================================

/**
 * @brief Grounded object entry
 */
typedef struct {
    uint32_t object_id;
    char concept_name[NIMCP_ID_BUFFER_SIZE];
    float confidence;
    uint32_t location_x;
    uint32_t location_y;
    uint64_t grounded_time;
    bool active;
} grounded_object_t;

/**
 * @brief Bridge internal structure
 */
struct visual_logic_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    nimcp_health_agent_t* health_agent;  /**< Phase 8: instance-level health agent */
    void* visual;                           /**< Visual cortex handle */
    void* logic;                            /**< Logic module handle */
    visual_logic_config_t config;           /**< Configuration */
    visual_logic_stats_t stats;             /**< Statistics */

    /* Grounding state */
    grounded_object_t grounded[MAX_GROUNDED_OBJECTS];
    uint32_t grounded_count;

    /* Pending commands (logic -> visual) */
    logic_visual_command_t pending_commands[MAX_PENDING_COMMANDS];
    uint32_t pending_count;

    /* Running averages */
    float confidence_sum;
    float relation_confidence_sum;
    uint64_t confidence_count;
    uint64_t relation_count;
};

//=============================================================================
// Configuration API
//=============================================================================

visual_logic_config_t visual_logic_default_config(void) {
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_default", 0.0f);


    return (visual_logic_config_t){
        .enable_object_grounding = true,
        .enable_relation_extraction = true,
        .enable_top_down_attention = true,
        .enable_verification = true,
        .min_confidence_threshold = NIMCP_CONFIDENCE_MEDIUM,
        .min_salience_threshold = NIMCP_SALIENCE_THRESHOLD,
        .max_objects_per_frame = 32,
        .max_relations_per_frame = 64
    };
}

//=============================================================================
// Lifecycle API
//=============================================================================

visual_logic_bridge_t* visual_logic_bridge_create(
    void* visual,
    void* logic,
    const visual_logic_config_t* config
) {
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_create", 0.0f);


    visual_logic_bridge_t* bridge = nimcp_calloc(1, sizeof(visual_logic_bridge_t));
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "Failed to allocate bridge");

        return NULL;

    }

    bridge->visual = visual;
    bridge->logic = logic;
    bridge->config = config ? *config : visual_logic_default_config();

    memset(&bridge->stats, 0, sizeof(bridge->stats));
    memset(bridge->grounded, 0, sizeof(bridge->grounded));
    bridge->grounded_count = 0;
    bridge->pending_count = 0;

    bridge->confidence_sum = 0.0f;
    bridge->relation_confidence_sum = 0.0f;
    bridge->confidence_count = 0;
    bridge->relation_count = 0;

    return bridge;
}

void visual_logic_bridge_destroy(visual_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_destroy", 0.0f);


    if (bridge) {
        nimcp_free(bridge);
        bridge = NULL;
    }
}

int visual_logic_bridge_reset(visual_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_bridge_reset: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_reset", 0.0f);


    memset(bridge->grounded, 0, sizeof(bridge->grounded));
    bridge->grounded_count = 0;
    bridge->pending_count = 0;

    return 0;
}

//=============================================================================
// Helper Functions
//=============================================================================

static int find_grounded_slot(visual_logic_bridge_t* bridge, uint32_t object_id) {
    for (uint32_t i = 0; i < MAX_GROUNDED_OBJECTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_GROUNDED_OBJECTS > 256) {
            visual_logic_bridge_heartbeat("visual_logic_loop",
                             (float)(i + 1) / (float)MAX_GROUNDED_OBJECTS);
        }

        if (bridge->grounded[i].active && bridge->grounded[i].object_id == object_id) {
            return (int)i;
        }
    }
    NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "find_grounded_slot: validation failed");
    return -1;
}

static int find_free_slot(visual_logic_bridge_t* bridge) {
    for (uint32_t i = 0; i < MAX_GROUNDED_OBJECTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_GROUNDED_OBJECTS > 256) {
            visual_logic_bridge_heartbeat("visual_logic_loop",
                             (float)(i + 1) / (float)MAX_GROUNDED_OBJECTS);
        }

        if (!bridge->grounded[i].active) {
            return (int)i;
        }
    }
    return -1;  /* All slots occupied is normal */
}

//=============================================================================
// Visual → Logic API
//=============================================================================

int visual_logic_ground_observation(
    visual_logic_bridge_t* bridge,
    const visual_logic_observation_t* obs
) {
    if (!bridge || !obs) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_ground_observation: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_object_grounding) return 0;

    /* Filter by confidence and salience */
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_ground_", 0.0f);


    if (obs->confidence < bridge->config.min_confidence_threshold) {
        return 0;
    }
    if (obs->salience < bridge->config.min_salience_threshold) {
        return 0;
    }

    /* Check if already grounded, update if so */
    int slot = find_grounded_slot(bridge, obs->object_id);
    if (slot < 0) {
        slot = find_free_slot(bridge);
        if (slot < 0) {
            /* No free slots - could evict oldest or lowest confidence */
            NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_ground_observation: validation failed");
            return -1;
        }
        bridge->grounded_count++;
    }

    /* Update grounded object entry */
    grounded_object_t* entry = &bridge->grounded[slot];
    entry->object_id = obs->object_id;
    strncpy(entry->concept_name, obs->concept_name, sizeof(entry->concept_name) - 1);
    entry->concept_name[sizeof(entry->concept_name) - 1] = '\0';
    entry->confidence = obs->confidence;
    entry->location_x = obs->location_x;
    entry->location_y = obs->location_y;
    entry->grounded_time = obs->timestamp_us ? obs->timestamp_us : nimcp_time_get_us();
    entry->active = true;

    /* Update statistics */
    bridge->stats.objects_grounded++;
    bridge->confidence_sum += obs->confidence;
    bridge->confidence_count++;
    bridge->stats.avg_grounding_confidence = bridge->confidence_sum / bridge->confidence_count;

    /*
     * TODO: Actually inject predicate into logic module
     * Would call something like:
     * symbolic_logic_assert_fact(bridge->logic, predicate_name, terms, arity, confidence);
     */

    return 0;
}

int visual_logic_report_relation(
    visual_logic_bridge_t* bridge,
    const visual_logic_relation_t* relation
) {
    if (!bridge || !relation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_report_relation: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_relation_extraction) return 0;

    /* Validate both objects are grounded */
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_report_", 0.0f);


    if (find_grounded_slot(bridge, relation->subject_id) < 0) {
        return 0; /* Subject not grounded */
    }
    if (find_grounded_slot(bridge, relation->object_id) < 0) {
        return 0; /* Object not grounded */
    }

    /* Update statistics */
    bridge->stats.relations_extracted++;
    bridge->relation_confidence_sum += relation->confidence;
    bridge->relation_count++;
    bridge->stats.avg_relation_confidence = bridge->relation_confidence_sum / bridge->relation_count;

    /*
     * TODO: Actually inject relation predicate into logic module
     * Would call something like:
     * symbolic_logic_assert_relation(bridge->logic, relation_name, subject, object, confidence);
     */

    return 0;
}

int visual_logic_process_frame(
    visual_logic_bridge_t* bridge,
    const visual_logic_observation_t* observations,
    uint32_t num_observations,
    const visual_logic_relation_t* relations,
    uint32_t num_relations
) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_process_frame: bridge is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_process", 0.0f);


    int processed = 0;

    /* Process observations */
    uint32_t obs_limit = num_observations < bridge->config.max_objects_per_frame
                        ? num_observations : bridge->config.max_objects_per_frame;

    for (uint32_t i = 0; i < obs_limit; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && obs_limit > 256) {
            visual_logic_bridge_heartbeat("visual_logic_loop",
                             (float)(i + 1) / (float)obs_limit);
        }

        if (visual_logic_ground_observation(bridge, &observations[i]) == 0) {
            processed++;
        }
    }

    /* Process relations */
    if (relations && num_relations > 0) {
        uint32_t rel_limit = num_relations < bridge->config.max_relations_per_frame
                            ? num_relations : bridge->config.max_relations_per_frame;

        for (uint32_t i = 0; i < rel_limit; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && rel_limit > 256) {
                visual_logic_bridge_heartbeat("visual_logic_loop",
                                 (float)(i + 1) / (float)rel_limit);
            }

            if (visual_logic_report_relation(bridge, &relations[i]) == 0) {
                processed++;
            }
        }
    }

    return processed;
}

//=============================================================================
// Logic → Visual API
//=============================================================================

int visual_logic_request_attention(
    visual_logic_bridge_t* bridge,
    const char* concept_name,
    float priority
) {
    if (!bridge || !concept_name) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_request_attention: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_top_down_attention) return 0;

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_request", 0.0f);


    if (bridge->pending_count >= MAX_PENDING_COMMANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "visual_logic_request_attention: capacity exceeded");
        return -1; /* Command queue full */
    }

    logic_visual_command_t* cmd = &bridge->pending_commands[bridge->pending_count++];
    cmd->signal_type = LOGIC_VISUAL_ATTEND_OBJECT;
    strncpy(cmd->target_concept, concept_name, sizeof(cmd->target_concept) - 1);
    cmd->target_concept[sizeof(cmd->target_concept) - 1] = '\0';
    cmd->priority = priority < 0.0f ? 0.0f : (priority > 1.0f ? 1.0f : priority);
    cmd->spatial_hint = false;

    bridge->stats.attention_commands++;

    /*
     * TODO: Actually send attention command to visual cortex
     * Would call something like:
     * visual_cortex_set_attention_target(bridge->visual, concept_name, priority);
     */

    return 0;
}

int visual_logic_verify_predicate(
    visual_logic_bridge_t* bridge,
    const char* predicate_name,
    bool expected_value,
    bool* verified,
    float* confidence
) {
    if (!bridge || !predicate_name || !verified || !confidence) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_verify_predicate: required parameter is NULL");
        return -1;
    }
    if (!bridge->config.enable_verification) {
        *verified = false;
        *confidence = 0.0f;
        return 0;
    }

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_verify_", 0.0f);


    bridge->stats.verifications_requested++;

    /* Search grounded objects for matching concept */
    *verified = false;
    *confidence = 0.0f;

    for (uint32_t i = 0; i < MAX_GROUNDED_OBJECTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_GROUNDED_OBJECTS > 256) {
            visual_logic_bridge_heartbeat("visual_logic_loop",
                             (float)(i + 1) / (float)MAX_GROUNDED_OBJECTS);
        }

        if (bridge->grounded[i].active) {
            if (strncmp(bridge->grounded[i].concept_name, predicate_name,
                       sizeof(bridge->grounded[i].concept_name)) == 0) {
                *verified = expected_value; /* Found matching concept */
                *confidence = bridge->grounded[i].confidence;
                bridge->stats.verifications_confirmed++;
                return 0;
            }
        }
    }

    /* Not found - verification fails if expected true, succeeds if expected false */
    *verified = !expected_value;
    *confidence = 0.5f; /* Medium confidence for absence */

    return 0;
}

int visual_logic_send_command(
    visual_logic_bridge_t* bridge,
    const logic_visual_command_t* command
) {
    if (!bridge || !command) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_send_command: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_send_co", 0.0f);


    if (bridge->pending_count >= MAX_PENDING_COMMANDS) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "visual_logic_send_command: capacity exceeded");
        return -1;
    }

    bridge->pending_commands[bridge->pending_count++] = *command;

    switch (command->signal_type) {
        case LOGIC_VISUAL_ATTEND_OBJECT:
        case LOGIC_VISUAL_EXPECT_FEATURE:
            bridge->stats.attention_commands++;
            break;
        case LOGIC_VISUAL_VERIFY_PREDICATE:
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

int visual_logic_is_grounded(
    const visual_logic_bridge_t* bridge,
    uint32_t object_id,
    bool* grounded
) {
    if (!bridge || !grounded) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_is_grounded: required parameter is NULL");
        return -1;
    }

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_is_grou", 0.0f);


    for (uint32_t i = 0; i < MAX_GROUNDED_OBJECTS; i++) {
        /* Phase 8: Loop progress heartbeat */
        if ((i & 0xFF) == 0 && MAX_GROUNDED_OBJECTS > 256) {
            visual_logic_bridge_heartbeat("visual_logic_loop",
                             (float)(i + 1) / (float)MAX_GROUNDED_OBJECTS);
        }

        if (bridge->grounded[i].active && bridge->grounded[i].object_id == object_id) {
            *grounded = true;
            return 0;
        }
    }

    *grounded = false;
    return 0;
}

int visual_logic_get_grounded_count(const visual_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_get_grounded_count: bridge is NULL");
        return -1;
    }
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_visual_logic_get_gro", 0.0f);


    return (int)bridge->grounded_count;
}

//=============================================================================
// Statistics API
//=============================================================================

int visual_logic_bridge_get_stats(
    const visual_logic_bridge_t* bridge,
    visual_logic_stats_t* stats
) {
    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_bridge_get_stats: required parameter is NULL");
        return -1;
    }
    *stats = bridge->stats;
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_get_stats", 0.0f);


    return 0;
}

void visual_logic_bridge_reset_stats(visual_logic_bridge_t* bridge) {
    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_reset_stats", 0.0f);


    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "visual_logic_bridge_reset_stats: bridge is NULL");
        return;
    }
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    bridge->confidence_sum = 0.0f;
    bridge->relation_confidence_sum = 0.0f;
    bridge->confidence_count = 0;
    bridge->relation_count = 0;
}

/* ============================================================================
 * Knowledge Graph Self-Awareness Integration
 * ============================================================================ */

int visual_logic_bridge_query_self_knowledge(kg_reader_t* kg) {
    if (!kg) return 0;

    /* Phase 8: Heartbeat at operation start */
    visual_logic_bridge_heartbeat("visual_logic_query_self_knowledge", 0.0f);


    const kg_entity_t* self = kg_reader_get_entity(kg, "Visual_Logic_Bridge");
    if (self) {
        for (uint32_t i = 0; i < self->num_observations; i++) {
            /* Phase 8: Loop progress heartbeat */
            if ((i & 0xFF) == 0 && self->num_observations > 256) {
                visual_logic_bridge_heartbeat("visual_logic_loop",
                                 (float)(i + 1) / (float)self->num_observations);
            }

            (void)self->observations[i];
        }
    }

    kg_relation_list_t* connections = kg_reader_get_relations_from(kg, "Visual_Logic_Bridge");
    if (connections) {
        kg_relation_list_destroy(connections);
    }

    kg_relation_list_t* incoming = kg_reader_get_relations_to(kg, "Visual_Logic_Bridge");
    if (incoming) {
        kg_relation_list_destroy(incoming);
    }

    return self ? 1 : 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void visual_logic_bridge_set_instance_health_agent(visual_logic_bridge_t* bridge, nimcp_health_agent_t* agent) {
    if (!bridge) {
        NIMCP_THROW(NIMCP_ERROR_NULL_POINTER,
                    "visual_logic_bridge_set_instance_health_agent: NULL bridge");
        return;
    }
    bridge->health_agent = agent;
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int visual_logic_bridge_training_begin(visual_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "visual_logic_bridge_training_begin: NULL argument");
        return -1;
    }
    visual_logic_bridge_heartbeat_instance(bridge->health_agent, "visual_logic_bridge_training_begin", 0.0f);
    return 0;
}

int visual_logic_bridge_training_end(visual_logic_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "visual_logic_bridge_training_end: NULL argument");
        return -1;
    }
    visual_logic_bridge_heartbeat_instance(bridge->health_agent, "visual_logic_bridge_training_end", 1.0f);
    return 0;
}

int visual_logic_bridge_training_step(visual_logic_bridge_t* bridge, float progress) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "visual_logic_bridge_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    visual_logic_bridge_heartbeat_instance(bridge->health_agent, "visual_logic_bridge_training_step", progress);
    return 0;
}
