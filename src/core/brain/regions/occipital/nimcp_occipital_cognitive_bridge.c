/**
 * @file nimcp_occipital_cognitive_bridge.c
 * @brief Implementation of Occipital-Cognitive unified bridge
 *
 * WHAT: Central integration point for occipital-cognitive communication
 * WHY: Enable visual perception to influence and be influenced by cognition
 * HOW: Routes visual information to all cognitive modules via bio-async
 *
 * @version Phase O1: Occipital Cognitive Integration
 * @author NIMCP Development Team
 * @date 2025-12-31
 */

#include "utils/bridge/nimcp_bridge_base.h"
#include "core/brain/regions/occipital/nimcp_occipital_cognitive_bridge.h"
#include "core/brain/regions/occipital/nimcp_occipital_adapter.h"
#include "cognitive/common/nimcp_metabolic_modulation.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "async/nimcp_bio_async.h"
#include "async/nimcp_bio_router.h"
#include "async/nimcp_bio_messages.h"
#include "utils/exception/nimcp_exception_macros.h"
#include <string.h>
#include <math.h>
#include <time.h>
#include "utils/fault_tolerance/nimcp_health_agent_macros.h"
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"
#include "utils/math/nimcp_math_helpers.h"

BRIDGE_BOILERPLATE_MESH_ONLY(occipital_cognitive_bridge, MESH_ADAPTER_CATEGORY_COGNITIVE)


#define LOG_MODULE "OCCIPITAL_COGNITIVE_BRIDGE"


/*=============================================================================
 * LOGGING MODULE IDENTIFIER
 *===========================================================================*/

#define COG_BRIDGE_LOG_MODULE "OCC_COGNITIVE"

/*=============================================================================
 * INTERNAL CONSTANTS
 *===========================================================================*/

#define MAX_PENDING_EVENTS 64
#define MAX_PENDING_MODULATIONS 32
#define FEATURE_VECTOR_SIZE 8

/*=============================================================================
 * INTERNAL STRUCTURES
 *===========================================================================*/

/**
 * @brief Module connection state
 */
typedef struct {
    void* handle;                  /**< Module handle */
    bool connected;                /**< Is connected */
    bool enabled;                  /**< Is enabled */
    float weight;                  /**< Connection weight */
    float last_activity;           /**< Last activity level */
    uint64_t last_update_us;       /**< Last update time */
} module_connection_t;

/**
 * @brief Pending event queue entry
 */
typedef struct {
    visual_cognitive_event_t event;
    bool pending;
} pending_event_t;

/**
 * @brief Pending modulation queue entry
 */
typedef struct {
    cognitive_modulation_t modulation;
    bool pending;
} pending_modulation_t;

/**
 * @brief Internal bridge structure
 */
struct occipital_cognitive_bridge {
    bridge_base_t base;              /**< MUST be first: base bridge infrastructure */
    /* Configuration */
    occipital_cognitive_config_t config;

    /* Connected modules */
    occipital_adapter_t* occipital;
    module_connection_t modules[COG_MODULE_COUNT];
    neural_substrate_t* substrate;
    bio_router_t router;

    /* Module ID for bio-async */
    uint32_t module_id;

    /* Event queues */
    pending_event_t event_queue[MAX_PENDING_EVENTS];
    uint32_t event_queue_head;
    uint32_t event_queue_tail;

    /* Modulation queues */
    pending_modulation_t mod_queue[MAX_PENDING_MODULATIONS];
    uint32_t mod_queue_head;
    uint32_t mod_queue_tail;

    /* Aggregated modulation state */
    cognitive_modulation_t current_modulation;

    /* Current effects */
    occipital_cognitive_effects_t effects;

    /* Statistics */
    occipital_cognitive_stats_t stats;

    /* Timing */
    uint64_t last_update_us;
    uint64_t last_process_us;
    uint64_t creation_time_us;
};

/*=============================================================================
 * HELPER FUNCTIONS
 *===========================================================================*/

/**
 * @brief Get current time in microseconds
 */
static uint64_t get_time_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000ULL + (uint64_t)ts.tv_nsec / 1000ULL;
}

/* Note: Message type and channel mapping for bio-async will be implemented
 * when the full bio-async infrastructure is integrated */

/**
 * @brief Enqueue event for processing
 */
static int enqueue_event(occipital_cognitive_bridge_t* bridge,
                         const visual_cognitive_event_t* event) {
    uint32_t next_tail = (bridge->event_queue_tail + 1) % MAX_PENDING_EVENTS;

    if (next_tail == bridge->event_queue_head) {
        /* Queue full — normal capacity limit, not an error worth alerting immune */
        return -1;
    }

    bridge->event_queue[bridge->event_queue_tail].event = *event;
    bridge->event_queue[bridge->event_queue_tail].pending = true;
    bridge->event_queue_tail = next_tail;

    return 0;
}

/**
 * @brief Enqueue modulation for processing
 */
static int enqueue_modulation(occipital_cognitive_bridge_t* bridge,
                              const cognitive_modulation_t* mod) {
    uint32_t next_tail = (bridge->mod_queue_tail + 1) % MAX_PENDING_MODULATIONS;

    if (next_tail == bridge->mod_queue_head) {
        /* Queue full — normal capacity limit, not an error worth alerting immune */
        return -1;
    }

    bridge->mod_queue[bridge->mod_queue_tail].modulation = *mod;
    bridge->mod_queue[bridge->mod_queue_tail].pending = true;
    bridge->mod_queue_tail = next_tail;

    return 0;
}

/**
 * @brief Process pending modulations
 */
static void process_modulations(occipital_cognitive_bridge_t* bridge) {
    /* Reset current modulation */
    memset(&bridge->current_modulation, 0, sizeof(bridge->current_modulation));
    float total_weight = 0.0f;

    /* Aggregate all pending modulations */
    while (bridge->mod_queue_head != bridge->mod_queue_tail) {
        pending_modulation_t* pm = &bridge->mod_queue[bridge->mod_queue_head];
        if (pm->pending) {
            const cognitive_modulation_t* m = &pm->modulation;
            float weight = bridge->modules[m->source].weight;

            bridge->current_modulation.attention_gain += m->attention_gain * weight;
            bridge->current_modulation.spatial_focus_x += m->spatial_focus_x * weight;
            bridge->current_modulation.spatial_focus_y += m->spatial_focus_y * weight;
            bridge->current_modulation.emotional_valence += m->emotional_valence * weight;
            bridge->current_modulation.emotional_arousal += m->emotional_arousal * weight;

            for (int i = 0; i < 8; i++) {
                bridge->current_modulation.feature_bias[i] += m->feature_bias[i] * weight;
            }

            total_weight += weight;
            pm->pending = false;

            bridge->stats.modulations_received[m->source]++;
            bridge->stats.total_modulations_received++;
        }
        bridge->mod_queue_head = (bridge->mod_queue_head + 1) % MAX_PENDING_MODULATIONS;
    }

    /* Normalize by total weight */
    if (total_weight > 0.0f) {
        bridge->current_modulation.attention_gain /= total_weight;
        bridge->current_modulation.spatial_focus_x /= total_weight;
        bridge->current_modulation.spatial_focus_y /= total_weight;
        bridge->current_modulation.emotional_valence /= total_weight;
        bridge->current_modulation.emotional_arousal /= total_weight;

        for (int i = 0; i < 8; i++) {
            bridge->current_modulation.feature_bias[i] /= total_weight;
        }
    }

    bridge->current_modulation.timestamp_us = get_time_us();
}

/**
 * @brief Send event via bio-async
 *
 * Note: Full bio-async message sending will be implemented when
 * the infrastructure is fully integrated. For now, track statistics.
 */
static int send_event_bio_async(occipital_cognitive_bridge_t* bridge,
                                const visual_cognitive_event_t* event) {
    if (!bridge->router || !bridge->config.enable_bio_async) {
        return 0;
    }

    /* TODO: Implement actual bio-async message sending */
    bridge->stats.messages_sent++;
    (void)event; /* Unused for now */

    return 0;
}

/*=============================================================================
 * BIO-ASYNC COMMUNICATION
 *===========================================================================*/

/* Note: Bio-async message handlers will be implemented when
 * the full bio-async infrastructure is integrated */

/*=============================================================================
 * CONFIGURATION API
 *===========================================================================*/

occipital_cognitive_config_t occipital_cognitive_default_config(void) {
    occipital_cognitive_config_t config = {0};

    /* Initialize all modules with defaults */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        config.modules[i].type = (cognitive_module_type_t)i;
        config.modules[i].enabled = true;
        config.modules[i].weight = 1.0f;
        config.modules[i].update_rate_hz = 30.0f;
    }

    /* Adjust weights for more important modules */
    config.modules[COG_MODULE_ATTENTION].weight = 1.5f;
    config.modules[COG_MODULE_EMOTION].weight = 1.3f;
    config.modules[COG_MODULE_SALIENCE].weight = 1.4f;

    /* Global settings */
    config.enable_bio_async = true;
    config.enable_bidirectional = true;
    config.global_gain = 1.0f;
    config.max_active_modules = 8;

    /* Visual feature routing */
    config.route_edges = true;
    config.route_color = true;
    config.route_motion = true;
    config.route_faces = true;
    config.route_objects = true;

    return config;
}

/*=============================================================================
 * LIFECYCLE API
 *===========================================================================*/

occipital_cognitive_bridge_t* occipital_cognitive_bridge_create(
    occipital_adapter_t* occipital,
    const occipital_cognitive_config_t* config) {

    if (!occipital) {
        LOG_ERROR(COG_BRIDGE_LOG_MODULE, "NULL occipital adapter");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital is NULL");
        return NULL;
    }

    occipital_cognitive_bridge_t* bridge =
        (occipital_cognitive_bridge_t*)nimcp_malloc(sizeof(*bridge));
    if (!bridge) {
        LOG_ERROR(COG_BRIDGE_LOG_MODULE, "Failed to allocate bridge");
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NO_MEMORY, "occipital_cognitive_bridge_create: allocation failed");
        return NULL;
    }

    memset(bridge, 0, sizeof(*bridge));

    if (bridge_base_init(&bridge->base, 0, "occipital_cognitive") != 0) { nimcp_free(bridge); return NULL; }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = occipital_cognitive_default_config();
    }

    bridge->occipital = occipital;

    /* Initialize module connections */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        bridge->modules[i].connected = false;
        bridge->modules[i].enabled = bridge->config.modules[i].enabled;
        bridge->modules[i].weight = bridge->config.modules[i].weight;
    }

    bridge->creation_time_us = get_time_us();
    bridge->last_update_us = bridge->creation_time_us;
    bridge->last_process_us = bridge->creation_time_us;

    bridge->module_id = BIO_MODULE_OCCIPITAL + 0x40;

    LOG_INFO(COG_BRIDGE_LOG_MODULE, "Cognitive bridge created");

    return bridge;
}

void occipital_cognitive_bridge_destroy(occipital_cognitive_bridge_t* bridge) {
    if (!bridge) return;
    NIMCP_LOGGING_DEBUG("Destroying %s bridge", "occipital_cognitive");

    LOG_INFO(COG_BRIDGE_LOG_MODULE, "Destroying cognitive bridge");

    bridge_base_cleanup(&bridge->base);
    nimcp_free(bridge);
}

int occipital_cognitive_bridge_reset(occipital_cognitive_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* Clear queues */
    bridge->event_queue_head = 0;
    bridge->event_queue_tail = 0;
    bridge->mod_queue_head = 0;
    bridge->mod_queue_tail = 0;

    /* Reset modulation state */
    memset(&bridge->current_modulation, 0, sizeof(bridge->current_modulation));

    /* Reset effects */
    memset(&bridge->effects, 0, sizeof(bridge->effects));

    /* Reset module activity */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        bridge->modules[i].last_activity = 0.0f;
    }

    bridge->last_update_us = get_time_us();

    LOG_DEBUG(COG_BRIDGE_LOG_MODULE, "Bridge reset");

    return 0;
}

/*=============================================================================
 * CONNECTION API
 *===========================================================================*/

int occipital_cognitive_connect_module(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type,
    void* module_handle) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (type >= COG_MODULE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "occipital_cognitive_bridge_reset: capacity exceeded");
        return -1;
    }

    bridge->modules[type].handle = module_handle;
    bridge->modules[type].connected = (module_handle != NULL);
    bridge->modules[type].last_update_us = get_time_us();

    LOG_INFO(COG_BRIDGE_LOG_MODULE, "Connected module %d", type);

    return 0;
}

int occipital_cognitive_disconnect_module(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }
    if (type >= COG_MODULE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "occipital_cognitive_bridge_reset: capacity exceeded");
        return -1;
    }

    bridge->modules[type].handle = NULL;
    bridge->modules[type].connected = false;

    LOG_INFO(COG_BRIDGE_LOG_MODULE, "Disconnected module %d", type);

    return 0;
}

int occipital_cognitive_bridge_register_bio_async(
    occipital_cognitive_bridge_t* bridge,
    struct bio_router_struct* router) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->router = router;

    if (router) {
        LOG_INFO(COG_BRIDGE_LOG_MODULE, "Registered with bio-async router");
    }

    return 0;
}

int occipital_cognitive_connect_substrate(
    occipital_cognitive_bridge_t* bridge,
    neural_substrate_t* substrate) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    bridge->substrate = substrate;

    LOG_INFO(COG_BRIDGE_LOG_MODULE, "Connected to neural substrate");

    return 0;
}

/*=============================================================================
 * EVENT API
 *===========================================================================*/

int occipital_cognitive_send_event(
    occipital_cognitive_bridge_t* bridge,
    const visual_cognitive_event_t* event) {

    if (!bridge || !event) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_cognitive_bridge_reset: required parameter is NULL (bridge, event)");
        return -1;
    }

    cognitive_module_type_t target = event->target;
    if (target >= COG_MODULE_COUNT) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_OUT_OF_RANGE, "occipital_cognitive_bridge_reset: capacity exceeded");
        return -1;
    }

    if (!bridge->modules[target].enabled) {
        return 0; /* Silently ignore disabled modules */
    }

    /* Update activity */
    bridge->modules[target].last_activity = event->salience;
    bridge->modules[target].last_update_us = get_time_us();

    /* Send via bio-async */
    int result = send_event_bio_async(bridge, event);

    /* Update stats */
    bridge->stats.events_sent[target]++;
    bridge->stats.total_events_sent++;

    return result;
}

int occipital_cognitive_broadcast(
    occipital_cognitive_bridge_t* bridge,
    const float* features,
    uint32_t feature_count,
    float salience) {

    if (!bridge || !features) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "occipital_cognitive_bridge_reset: required parameter is NULL (bridge, features)");
        return -1;
    }

    int modules_notified = 0;
    uint64_t now = get_time_us();

    visual_cognitive_event_t event = {
        .event_type = 0, /* Generic visual event */
        .salience = salience,
        .urgency = salience * 0.5f,
        .timestamp_us = now
    };

    /* Copy features */
    uint32_t copy_count = feature_count < FEATURE_VECTOR_SIZE ?
                          feature_count : FEATURE_VECTOR_SIZE;
    memcpy(event.visual_features, features, copy_count * sizeof(float));

    /* Send to all enabled modules */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        if (bridge->modules[i].enabled &&
            (bridge->modules[i].connected || bridge->router)) {
            event.target = (cognitive_module_type_t)i;

            /* Apply weight to salience */
            float weighted_salience = salience * bridge->modules[i].weight;
            if (weighted_salience >= 0.1f) {
                event.salience = weighted_salience;
                occipital_cognitive_send_event(bridge, &event);
                modules_notified++;
            }
        }
    }

    return modules_notified;
}

int occipital_cognitive_apply_modulation(
    occipital_cognitive_bridge_t* bridge,
    const cognitive_modulation_t* modulation) {

    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "unknown: required parameter is NULL (bridge, modulation)");
        return -1;
    }

    if (!bridge->config.enable_bidirectional) {
        return 0;
    }

    return enqueue_modulation(bridge, modulation);
}

/*=============================================================================
 * PROCESSING API
 *===========================================================================*/

int occipital_cognitive_bridge_update(occipital_cognitive_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    uint64_t now = get_time_us();

    /* Process pending modulations */
    process_modulations(bridge);

    /* Update effects */
    bridge->effects.total_top_down_gain = bridge->current_modulation.attention_gain;
    bridge->effects.emotional_modulation = bridge->current_modulation.emotional_valence;

    float attention_focus = sqrtf(
        bridge->current_modulation.spatial_focus_x *
        bridge->current_modulation.spatial_focus_x +
        bridge->current_modulation.spatial_focus_y *
        bridge->current_modulation.spatial_focus_y
    );
    bridge->effects.attention_focus = nimcp_clampf(attention_focus, 0.0f, 1.0f);

    /* Calculate cognitive load */
    float total_activity = 0.0f;
    uint32_t active_count = 0;
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        bridge->effects.module_activity[i] = bridge->modules[i].last_activity;
        if (bridge->modules[i].last_activity > 0.1f) {
            total_activity += bridge->modules[i].last_activity;
            active_count++;
        }
    }

    if (active_count > 0) {
        bridge->effects.cognitive_load = total_activity / (float)COG_MODULE_COUNT;
    }

    /* Visual-cognitive sync based on modulation recency */
    uint64_t mod_age = now - bridge->current_modulation.timestamp_us;
    float sync = expf(-(float)mod_age / 100000.0f); /* 100ms decay */
    bridge->effects.visual_cognitive_sync = sync;

    /* Module-specific effects */
    bridge->effects.curiosity_drive = bridge->modules[COG_MODULE_CURIOSITY].last_activity;
    bridge->effects.salience_enhancement = bridge->modules[COG_MODULE_SALIENCE].last_activity;
    bridge->effects.memory_retrieval_boost = bridge->modules[COG_MODULE_MEMORY].last_activity;

    bridge->last_update_us = now;

    return 0;
}

int occipital_cognitive_bridge_process(occipital_cognitive_bridge_t* bridge) {
    if (!bridge) {

        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");

        return -1;

    }

    /* In a full implementation, this would:
       1. Query occipital adapter for current visual features
       2. Extract relevant features based on routing config
       3. Route features to appropriate cognitive modules
       4. Handle priority based on salience

       For now, simulate with placeholder processing */

    uint64_t now = get_time_us();
    float dt_ms = (float)(now - bridge->last_process_us) / 1000.0f;

    /* Decay module activities */
    float decay = expf(-dt_ms / 500.0f); /* 500ms half-life */
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        bridge->modules[i].last_activity *= decay;
    }

    bridge->last_process_us = now;

    return 0;
}

int occipital_cognitive_bridge_get_effects(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_effects_t* effects) {

    if (!bridge || !effects) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_bridge_get_effects: required parameter is NULL");
        return -1;
    }

    *effects = bridge->effects;

    return 0;
}

int occipital_cognitive_get_modulation(
    const occipital_cognitive_bridge_t* bridge,
    cognitive_modulation_t* modulation) {

    if (!bridge || !modulation) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_get_modulation: required parameter is NULL");
        return -1;
    }

    *modulation = bridge->current_modulation;

    return 0;
}

/*=============================================================================
 * MODULE-SPECIFIC API
 *===========================================================================*/

int occipital_cognitive_send_emotion_cue(
    occipital_cognitive_bridge_t* bridge,
    float valence,
    float arousal,
    uint32_t expression_id,
    float confidence) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    visual_cognitive_event_t event = {
        .target = COG_MODULE_EMOTION,
        .event_type = expression_id,
        .visual_features = {valence, arousal, confidence, 0, 0, 0, 0, 0},
        .salience = fabsf(valence) * arousal,
        .urgency = arousal,
        .timestamp_us = get_time_us()
    };

    return occipital_cognitive_send_event(bridge, &event);
}

int occipital_cognitive_report_salience(
    occipital_cognitive_bridge_t* bridge,
    float x, float y,
    float salience,
    uint32_t feature_type) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    visual_cognitive_event_t event = {
        .target = COG_MODULE_SALIENCE,
        .event_type = feature_type,
        .visual_features = {x, y, salience, 0, 0, 0, 0, 0},
        .salience = salience,
        .urgency = salience * 0.8f,
        .timestamp_us = get_time_us()
    };

    return occipital_cognitive_send_event(bridge, &event);
}

int occipital_cognitive_query_memory(
    occipital_cognitive_bridge_t* bridge,
    const float* pattern,
    uint32_t pattern_size,
    float* match_score,
    uint32_t* match_id) {

    if (!bridge || !pattern) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_query_memory: required parameter is NULL");
        return -1;
    }

    visual_cognitive_event_t event = {
        .target = COG_MODULE_MEMORY,
        .event_type = 1, /* Query type */
        .salience = 0.5f,
        .urgency = 0.3f,
        .timestamp_us = get_time_us()
    };

    uint32_t copy = pattern_size < 8 ? pattern_size : 8;
    memcpy(event.visual_features, pattern, copy * sizeof(float));

    occipital_cognitive_send_event(bridge, &event);

    /* In a real implementation, this would wait for response */
    if (match_score) *match_score = 0.0f;
    if (match_id) *match_id = 0;

    bridge->stats.events_sent[COG_MODULE_MEMORY]++;

    return 0;
}

int occipital_cognitive_report_novelty(
    occipital_cognitive_bridge_t* bridge,
    float novelty_score,
    const float* features,
    uint32_t feature_count) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER, "bridge is NULL");
        return -1;
    }

    visual_cognitive_event_t event = {
        .target = COG_MODULE_CURIOSITY,
        .event_type = 0,
        .salience = novelty_score,
        .urgency = novelty_score * 0.6f,
        .timestamp_us = get_time_us()
    };

    if (features && feature_count > 0) {
        uint32_t copy = feature_count < 8 ? feature_count : 8;
        memcpy(event.visual_features, features, copy * sizeof(float));
    }
    event.visual_features[7] = novelty_score;

    return occipital_cognitive_send_event(bridge, &event);
}

/*=============================================================================
 * QUERY API
 *===========================================================================*/

int occipital_cognitive_bridge_get_stats(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_stats_t* stats) {

    if (!bridge || !stats) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_bridge_get_stats: required parameter is NULL");
        return -1;
    }

    *stats = bridge->stats;

    return 0;
}

void occipital_cognitive_bridge_reset_stats(occipital_cognitive_bridge_t* bridge) {
    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_bridge_reset_stats: bridge is NULL");
        return;
    }

    memset(&bridge->stats, 0, sizeof(bridge->stats));
}

bool occipital_cognitive_is_module_connected(
    const occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type) {

    if (!bridge) {
        return false;
    }
    if (type >= COG_MODULE_COUNT) return false;

    return bridge->modules[type].connected;
}

uint32_t occipital_cognitive_get_active_module_count(
    const occipital_cognitive_bridge_t* bridge) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_get_active_module_count: bridge is NULL");
        return 0;
    }

    uint32_t count = 0;
    for (int i = 0; i < COG_MODULE_COUNT; i++) {
        if (bridge->modules[i].enabled &&
            (bridge->modules[i].connected || bridge->router)) {
            count++;
        }
    }

    return count;
}

int occipital_cognitive_bridge_get_config(
    const occipital_cognitive_bridge_t* bridge,
    occipital_cognitive_config_t* config) {

    if (!bridge || !config) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_bridge_get_config: required parameter is NULL");
        return -1;
    }

    *config = bridge->config;

    return 0;
}

int occipital_cognitive_set_module_enabled(
    occipital_cognitive_bridge_t* bridge,
    cognitive_module_type_t type,
    bool enabled) {

    if (!bridge) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_PARAM, "occipital_cognitive_set_module_enabled: bridge is NULL");
        return -1;
    }
    if (type >= COG_MODULE_COUNT) return -1;

    bridge->modules[type].enabled = enabled;
    bridge->config.modules[type].enabled = enabled;

    LOG_DEBUG(COG_BRIDGE_LOG_MODULE, "Module %d enabled=%d", type, enabled);

    return 0;
}
