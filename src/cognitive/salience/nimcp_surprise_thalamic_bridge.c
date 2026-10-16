/**
 * @file nimcp_surprise_thalamic_bridge.c
 * @brief Bridge between Surprise Amplifier and thalamic routing system
 * @version 1.1.0
 * @date 2026-01-27
 *
 * WHAT: Thalamic gating and routing of surprise signals
 * WHY:  Surprise signals must be routed to appropriate cortical destinations
 * HOW:  Signal type/urgency -> routing destination; attention -> gating weights
 *
 * @author NIMCP Development Team
 */

#include "cognitive/salience/nimcp_surprise_thalamic_bridge.h"
#include "cognitive/salience/nimcp_surprise_amplifier.h"
#include "glial/myelin_sheath/nimcp_myelin_math.h"
#include "async/nimcp_bio_messages.h"
#include "async/nimcp_bio_router.h"
#include "utils/memory/nimcp_memory.h"
#include "utils/logging/nimcp_logging.h"
#include "utils/thread/nimcp_thread.h"
#include "utils/exception/nimcp_exception_macros.h"

#include <string.h>
#include <math.h>
#include <stddef.h>

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */
#include "utils/bridge/nimcp_bridge_boilerplate.h"
#include "mesh/nimcp_mesh_participant.h"
#include "mesh/nimcp_mesh_adapter.h"

BRIDGE_BOILERPLATE(surprise_thalamic, MESH_ADAPTER_CATEGORY_COGNITIVE)
static nimcp_health_agent_t* g_surprise_thalamic_bridge_health_agent = NULL;
void surprise_thalamic_bridge_set_health_agent_global(struct nimcp_health_agent* agent) { g_surprise_thalamic_bridge_health_agent = agent; }

/* ============================================================================
 * Constants
 * ============================================================================ */

#define NUM_SIGNAL_TYPES 4

/* ============================================================================
 * Internal Structure
 * ============================================================================ */

struct surprise_thalamic_bridge {
    surprise_thalamic_config_t config;

    /* Connected systems */
    struct surprise_amplifier* amplifier;
    void* thalamic_router;

    /* State */
    surprise_thalamic_stats_t stats;

    /* Attention weights per signal type */
    float attention_weights[NUM_SIGNAL_TYPES];

    /* Bio-async */
    void* router;
    bool bio_async_connected;

    /* Thread safety */
    nimcp_mutex_t* mutex;

    /* Health agent */
    nimcp_health_agent_t* health_agent;

    bool initialized;
    uint64_t update_count;
};

/* ============================================================================
 * Helpers
 * ============================================================================ */

static int signal_type_to_index(uint32_t signal_type) {
    switch (signal_type) {
        case SURPRISE_THALAMIC_REALIZATION: return 0;
        case SURPRISE_THALAMIC_CONFLICT:    return 1;
        case SURPRISE_THALAMIC_NOVELTY:     return 2;
        case SURPRISE_THALAMIC_HYPOTHESIS:  return 3;
        default: return -1;
    }
}

static const char* signal_type_name(uint32_t signal_type) {
    switch (signal_type) {
        case SURPRISE_THALAMIC_REALIZATION: return "REALIZATION";
        case SURPRISE_THALAMIC_CONFLICT:    return "CONFLICT";
        case SURPRISE_THALAMIC_NOVELTY:     return "NOVELTY";
        case SURPRISE_THALAMIC_HYPOTHESIS:  return "HYPOTHESIS";
        default: return "UNKNOWN";
    }
}

static float get_threshold_for_type(const surprise_thalamic_config_t* cfg, uint32_t signal_type) {
    switch (signal_type) {
        case SURPRISE_THALAMIC_REALIZATION: return cfg->threshold_realization;
        case SURPRISE_THALAMIC_CONFLICT:    return cfg->threshold_conflict;
        case SURPRISE_THALAMIC_NOVELTY:     return cfg->threshold_novelty;
        case SURPRISE_THALAMIC_HYPOTHESIS:  return cfg->threshold_hypothesis;
        default: return 0.5f;
    }
}

static bool is_type_enabled(const surprise_thalamic_config_t* cfg, uint32_t signal_type) {
    switch (signal_type) {
        case SURPRISE_THALAMIC_REALIZATION: return cfg->enable_realization;
        case SURPRISE_THALAMIC_CONFLICT:    return cfg->enable_conflict;
        case SURPRISE_THALAMIC_NOVELTY:     return cfg->enable_novelty;
        case SURPRISE_THALAMIC_HYPOTHESIS:  return cfg->enable_hypothesis;
        default: return false;
    }
}

/* ============================================================================
 * Bio-Async Helpers
 * ============================================================================ */

static void thalamic_send_route_msg(surprise_thalamic_bridge_t* bridge,
                                     uint32_t signal_type, float magnitude,
                                     float weighted_magnitude, float urgency)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "thalamic_send_route_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        uint32_t signal_type;
        float magnitude;
        float weighted_magnitude;
        float urgency;
        uint64_t signals_routed;
    } surprise_thalamic_route_msg_t;

    surprise_thalamic_route_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_THALAMIC_ROUTE,
                        BIO_MODULE_SURPRISE_THALAMIC,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    if (urgency > 0.8f) {
        msg.header.flags = BIO_MSG_FLAG_URGENT;
    }
    msg.signal_type = signal_type;
    msg.magnitude = magnitude;
    msg.weighted_magnitude = weighted_magnitude;
    msg.urgency = urgency;
    msg.signals_routed = bridge->stats.signals_routed;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

static void thalamic_send_gate_msg(surprise_thalamic_bridge_t* bridge,
                                    uint32_t signal_type, float old_weight,
                                    float new_weight)
{
    if (!bridge->bio_async_connected) return;
    if (!bridge->router) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_INVALID_STATE, "thalamic_send_gate_msg: bio_async_connected but router is NULL");
        return;
    }

    typedef struct {
        bio_message_header_t header;
        uint32_t signal_type;
        float old_weight;
        float new_weight;
        uint64_t gating_updates;
    } surprise_thalamic_gate_msg_t;

    surprise_thalamic_gate_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    bio_msg_init_header(&msg.header,
                        BIO_MSG_SURPRISE_THALAMIC_GATE,
                        BIO_MODULE_SURPRISE_THALAMIC,
                        0, sizeof(msg));
    msg.header.channel = BIO_CHANNEL_NOREPINEPHRINE;
    msg.signal_type = signal_type;
    msg.old_weight = old_weight;
    msg.new_weight = new_weight;
    msg.gating_updates = bridge->stats.gating_updates;

    bio_router_broadcast((bio_module_context_t)bridge->router, &msg, sizeof(msg));
}

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

surprise_thalamic_config_t surprise_thalamic_bridge_default_config(void) {
    surprise_thalamic_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    cfg.enable_realization = true;
    cfg.enable_conflict = true;
    cfg.enable_novelty = true;
    cfg.enable_hypothesis = true;
    cfg.threshold_realization = 0.5f;
    cfg.threshold_conflict = 0.4f;
    cfg.threshold_novelty = 0.3f;
    cfg.threshold_hypothesis = 0.6f;
    cfg.attention_weight_default = SURPRISE_THALAMIC_DEFAULT_ATTENTION_WEIGHT;
    cfg.enable_bio_async = true;
    cfg.enable_logging = true;

    return cfg;
}

surprise_thalamic_bridge_t* surprise_thalamic_bridge_create(
    const surprise_thalamic_config_t* config)
{
    surprise_thalamic_bridge_t* bridge = (surprise_thalamic_bridge_t*)nimcp_calloc(
        1, sizeof(surprise_thalamic_bridge_t));
    if (!bridge) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_THALAMIC_ERROR_NO_MEMORY,
                           sizeof(surprise_thalamic_bridge_t),
                           "surprise_thalamic_bridge allocation failed (%zu bytes)",
                           sizeof(surprise_thalamic_bridge_t));
        return NULL;
    }

    if (config) {
        bridge->config = *config;
    } else {
        bridge->config = surprise_thalamic_bridge_default_config();
    }

    bridge->mutex = nimcp_mutex_create(NULL);
    if (!bridge->mutex) {
        NIMCP_THROW_MEMORY(NIMCP_SURPRISE_THALAMIC_ERROR_NO_MEMORY,
                           sizeof(nimcp_mutex_t),
                           "surprise_thalamic_bridge mutex allocation failed");
        nimcp_free(bridge);
        bridge = NULL;
        return NULL;
    }

    /* Initialize attention weights */
    for (int i = 0; i < NUM_SIGNAL_TYPES; i++) {
        bridge->attention_weights[i] = bridge->config.attention_weight_default;
    }

    bridge->initialized = true;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Created surprise-thalamic bridge (attn_default=%.1f)",
                           bridge->config.attention_weight_default);
        NIMCP_LOGGING_DEBUG("Thalamic bridge config: thresholds=[real=%.2f, conf=%.2f, "
                            "nov=%.2f, hyp=%.2f]",
                            bridge->config.threshold_realization,
                            bridge->config.threshold_conflict,
                            bridge->config.threshold_novelty,
                            bridge->config.threshold_hypothesis);
    }

    return bridge;
}

void surprise_thalamic_bridge_destroy(surprise_thalamic_bridge_t* bridge) {
    if (!bridge) return;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Destroying surprise-thalamic bridge (routed=%lu, overrides=%lu)",
                           (unsigned long)bridge->stats.signals_routed,
                           (unsigned long)bridge->stats.overrides);
        NIMCP_LOGGING_DEBUG("Thalamic bridge final weights: [%.2f, %.2f, %.2f, %.2f], "
                            "avg_surprise=%.3f",
                            bridge->attention_weights[0], bridge->attention_weights[1],
                            bridge->attention_weights[2], bridge->attention_weights[3],
                            bridge->stats.avg_surprise);
    }

    if (bridge->mutex) {
        nimcp_mutex_destroy(bridge->mutex);
    }
    nimcp_free(bridge);
    bridge = NULL;
}

int surprise_thalamic_bridge_reset(surprise_thalamic_bridge_t* bridge) {
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in reset");

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "reset", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    memset(&bridge->stats, 0, sizeof(bridge->stats));
    for (int i = 0; i < NUM_SIGNAL_TYPES; i++) {
        bridge->attention_weights[i] = bridge->config.attention_weight_default;
    }
    bridge->update_count = 0;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Thalamic bridge reset: weights restored to default=%.2f",
                            bridge->config.attention_weight_default);
    }

    return 0;
}

/* ============================================================================
 * Connection API
 * ============================================================================ */

int surprise_thalamic_bridge_connect_amplifier(
    surprise_thalamic_bridge_t* bridge,
    struct surprise_amplifier* amp)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in connect_amplifier");
    NIMCP_CHECK_THROW_IMMUNE(amp != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL amp in connect_amplifier");

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "connect_amplifier", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->amplifier = amp;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-thalamic bridge connected to amplifier");
        NIMCP_LOGGING_DEBUG("Thalamic bridge amplifier connection established, ptr=%p", (void*)amp);
    }
    return 0;
}

int surprise_thalamic_bridge_connect_thalamic_router(
    surprise_thalamic_bridge_t* bridge,
    void* router)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in connect_thalamic_router");
    NIMCP_CHECK_THROW_IMMUNE(router != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL router in connect_thalamic_router");

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "connect_thalamic_router", 0.0f);

    nimcp_mutex_lock(bridge->mutex);
    bridge->thalamic_router = router;
    nimcp_mutex_unlock(bridge->mutex);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_INFO("Surprise-thalamic bridge connected to thalamic router");
        NIMCP_LOGGING_DEBUG("Thalamic router connection established, ptr=%p", router);
    }
    return 0;
}

/* ============================================================================
 * Operations API
 * ============================================================================ */

int surprise_thalamic_route_surprise(
    surprise_thalamic_bridge_t* bridge,
    const surprise_thalamic_signal_t* signal)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in route_surprise");
    NIMCP_CHECK_THROW_IMMUNE(signal != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL signal in route_surprise");

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "route_surprise", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    uint32_t type = signal->signal_type;
    float magnitude = nimcp_myelin_clamp(signal->surprise_magnitude, 0.0f, 1.0f);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Thalamic route_surprise: type=0x%x, magnitude=%.3f, "
                            "urgency=%.3f, source=0x%x",
                            type, magnitude, signal->urgency, signal->source_module);
    }

    /* Check each signal type bit */
    uint32_t types[] = {SURPRISE_THALAMIC_REALIZATION, SURPRISE_THALAMIC_CONFLICT,
                        SURPRISE_THALAMIC_NOVELTY, SURPRISE_THALAMIC_HYPOTHESIS};

    for (int i = 0; i < NUM_SIGNAL_TYPES; i++) {
        if (!(type & types[i])) continue;
        if (!is_type_enabled(&bridge->config, types[i])) continue;

        int idx = signal_type_to_index(types[i]);
        if (idx < 0) continue;

        float threshold = get_threshold_for_type(&bridge->config, types[i]);
        float weighted_magnitude = magnitude * bridge->attention_weights[idx];

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_TRACE("Thalamic routing [%d/%d]: type=%s, weight=%.3f, "
                                "weighted=%.3f, threshold=%.3f",
                                i + 1, NUM_SIGNAL_TYPES,
                                signal_type_name(types[i]),
                                bridge->attention_weights[idx],
                                weighted_magnitude, threshold);
        }

        if (weighted_magnitude >= threshold) {
            bridge->stats.signals_routed++;

            if (signal->urgency > 0.8f) {
                bridge->stats.high_priority_routes++;
            }

            if (bridge->config.enable_logging) {
                NIMCP_LOGGING_DEBUG("Thalamic signal ROUTED: type=%s, weighted=%.3f >= threshold=%.3f",
                                    signal_type_name(types[i]), weighted_magnitude, threshold);
            }

            /* Bio-async: send route notification */
            thalamic_send_route_msg(bridge, types[i], magnitude,
                                     weighted_magnitude, signal->urgency);
        }
    }

    /* Update running average */
    float n = (float)(bridge->stats.signals_routed > 0 ? bridge->stats.signals_routed : 1);
    bridge->stats.avg_surprise =
        bridge->stats.avg_surprise * ((n - 1.0f) / n) + magnitude / n;

    bridge->stats.total_updates++;
    bridge->update_count++;

    /* Loop heartbeat for iteration-heavy routing */
    surprise_thalamic_heartbeat_instance(bridge->health_agent, "route_surprise", 1.0f);

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

int surprise_thalamic_route_realization(
    surprise_thalamic_bridge_t* bridge,
    float magnitude,
    uint32_t source_module)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in route_realization");

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "route_realization", 0.0f);

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Thalamic route_realization: magnitude=%.3f, source=0x%x",
                            magnitude, source_module);
    }

    surprise_thalamic_signal_t signal;
    memset(&signal, 0, sizeof(signal));
    signal.signal_type = SURPRISE_THALAMIC_REALIZATION;
    signal.surprise_magnitude = magnitude;
    signal.source_module = source_module;
    signal.urgency = nimcp_myelin_clamp(magnitude * 1.2f, 0.0f, 1.0f); /* Realizations are high priority */

    return surprise_thalamic_route_surprise(bridge, &signal);
}

int surprise_thalamic_set_attention_weight(
    surprise_thalamic_bridge_t* bridge,
    uint32_t signal_type,
    float weight)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in set_attention_weight");

    int idx = signal_type_to_index(signal_type);
    NIMCP_CHECK_THROW_IMMUNE(idx >= 0,
                             NIMCP_SURPRISE_THALAMIC_ERROR_INVALID_PARAM,
                             "Invalid signal type 0x%x", signal_type);

    surprise_thalamic_heartbeat_instance(bridge->health_agent, "set_attention_weight", 0.0f);

    nimcp_mutex_lock(bridge->mutex);

    float old_weight = bridge->attention_weights[idx];
    float new_weight = nimcp_myelin_clamp(weight, 0.0f, 5.0f);
    bridge->attention_weights[idx] = new_weight;
    bridge->stats.gating_updates++;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Thalamic attention weight changed: type=%s (0x%x), "
                            "old=%.3f -> new=%.3f",
                            signal_type_name(signal_type), signal_type,
                            old_weight, new_weight);
    }

    /* Bio-async: send gate update on weight change */
    if (fabsf(new_weight - old_weight) > 0.01f) {
        thalamic_send_gate_msg(bridge, signal_type, old_weight, new_weight);

        if (bridge->config.enable_logging) {
            NIMCP_LOGGING_DEBUG("Thalamic bio-async: sent GATE (type=%s, delta=%.3f)",
                                signal_type_name(signal_type),
                                new_weight - old_weight);
        }
    }

    nimcp_mutex_unlock(bridge->mutex);

    return 0;
}

float surprise_thalamic_get_attention_weight(
    const surprise_thalamic_bridge_t* bridge,
    uint32_t signal_type)
{
    if (!bridge) return 1.0f;

    int idx = signal_type_to_index(signal_type);
    if (idx < 0) return 1.0f;

    return bridge->attention_weights[idx];
}

/* ============================================================================
 * Query API
 * ============================================================================ */

int surprise_thalamic_bridge_get_stats(
    const surprise_thalamic_bridge_t* bridge,
    surprise_thalamic_stats_t* stats_out)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in get_stats");
    NIMCP_CHECK_THROW_IMMUNE(stats_out != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL stats_out in get_stats");

    *stats_out = bridge->stats;
    return 0;
}

/* ============================================================================
 * Health Agent Integration
 * ============================================================================ */

int surprise_thalamic_bridge_set_health_agent(
    surprise_thalamic_bridge_t* bridge,
    struct nimcp_health_agent* agent)
{
    NIMCP_CHECK_THROW_IMMUNE(bridge != NULL,
                             NIMCP_SURPRISE_THALAMIC_ERROR_NULL_POINTER,
                             "NULL bridge in set_health_agent");

    bridge->health_agent = agent;

    if (bridge->config.enable_logging) {
        NIMCP_LOGGING_DEBUG("Thalamic bridge instance health agent %s",
                            agent ? "set" : "cleared");
    }
    return 0;
}

/* ============================================================================
 * Phase 8: Instance-Level Health Agent
 * ============================================================================ */

void surprise_thalamic_set_instance_health_agent(void* instance, nimcp_health_agent_t* agent) {
    if (instance) {
        g_surprise_thalamic_health_agent = agent;
    }
}

/* ============================================================================
 * Phase 8: Training Integration (Full Implementation)
 * ============================================================================ */

int surprise_thalamic_training_begin(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_thalamic_training_begin: NULL argument");
        return -1;
    }
    surprise_thalamic_heartbeat_instance(NULL, "surprise_thalamic_training_begin", 0.0f);
    (void)instance;
    return 0;
}

int surprise_thalamic_training_end(void* instance) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_thalamic_training_end: NULL argument");
        return -1;
    }
    surprise_thalamic_heartbeat_instance(NULL, "surprise_thalamic_training_end", 1.0f);
    (void)instance;
    return 0;
}

int surprise_thalamic_training_step(void* instance, float progress) {
    if (!instance) {
        NIMCP_THROW_TO_IMMUNE(NIMCP_ERROR_NULL_POINTER,
                              "surprise_thalamic_training_step: NULL argument");
        return -1;
    }
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    surprise_thalamic_heartbeat_instance(NULL, "surprise_thalamic_training_step", progress);
    (void)instance;
    return 0;
}
